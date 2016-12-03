/*
 Copyright (c) 2012-2014  Kirill Belyaev
 * kirillbelyaev@yahoo.com
 * kirill@cs.colostate.edu
 * TeleScope - XML Message Stream Broker/Replicator Platform
 * This work is licensed under the Creative Commons Attribution-NonCommercial 3.0 Unported License. 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/ or send 
 * a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
 */

/*
 "DION
 The violent carriage of it
 Will clear or end the business: when the oracle,
 Thus by Apollo's great divine seal'd up,
 Shall the contents discover, something rare
 Even then will rush to knowledge. Go: fresh horses!
 And gracious be the issue!"

 Winter's Tale, Act 3, Scene 1. William Shakespeare
 */

#include "parserEngine.h"

//parsing engine globals
bool Global_AND = true;
bool Global_OR;
bool complex = false;
int Exp_Global_Number;

char Expression[MAX_LINE];

//REG_EXP
char reg_start[MIN_LINE];
char regex[MIN_LINE];

//arrays for holding parsed cmd search expressions
char tuple[1024][INET_ADDRSTRLEN];
char logic[MIN_LINE];
char element[MIN_LINE][MIN_LINE];
char operator[MIN_LINE];
char value[MIN_LINE][MIN_LINE];
char content[MIN_LINE];

//parsing engine globals
int truth[MIN_LINE];
char plogic[MIN_LINE];
char expression_Global[MIN_LINE][MAX_LINE];

int tokenize( char *exp );
int shuffle2( int exp_gn );
int shuffle( int exp_gn );
int harvest( char *exp, xmlNode *root_element );
int clearExpGlobals( void );
int resetMatch( void );

//search flags for IP prefix search function
int exactflag = 0;
int moreflag = 0;
int lessflag = 0;

///cycles through xml object and gets all the elements and their  digit or string values.
int get_element(xmlNode * a_node, char *element_name, char operator, char *value);

///cycles through xml object and gets all the attributes and their  digit or string values.
int get_attribute(xmlNode * a_node, char *attribute_name, char operator, char *value);

///checks whether the IP is within a specified IP range
int ip_range( char *, char *, uint32_t pn, uint32_t cn );

///cycles through xml object and gets all the PREFIX elements values.
int get_prefix(xmlNode * a_node, char *element_name, char operator, char *value);

///fills the expression arrays with data to drive the exp engine
int fill( char * exp );

/// @brief Access the number of logical expressions in the parser engine.
/// @return Returns the number of logical expressions.
int getNumLogicalExpressions();

int clearExpGlobals( void )
{
    int i = 0;

    memset( Expression, '\0', sizeof( Expression ) );

    memset( truth, '\0', sizeof( truth ) );

    for ( i = 0; i < 256; i++ )
    {
        truth[i] = 0;
    }

    memset( plogic, '\0', sizeof( plogic ) );

    for ( i = 0; i < 256; i++ )
    {
        plogic[i] = '\0';
    }

    for ( i = 0; i < 256; i++ )
    {
        memset( expression_Global[i], '\0', sizeof( expression_Global[i] ) );
        strcpy( expression_Global[i], "" );
    }

    //clear the globals
    for ( i = 0; i < 256; i++ )
    {
        memset( element[i], '\0', sizeof( element[i] ) );
        operator[i] = '\0';
        memset( value[i], '\0', sizeof( value[i] ) );
        logic[i] = '\0';
    }

    for ( i = 0; i < 1024; i++ )
    {
        memset( tuple[i], '\0', sizeof( tuple[i] ) );
    }
    return 0;
}

int InitializeParseEngine( char *evalue )
{
    int i, l, k;

    resetMatch();        //02/17/13

    clearExpGlobals(); //05/10/12 clear the exp globals arrays

    if ( strlen( evalue ) > MAX_LINE )/* 09/12/11 - added extra security check */
    {
        perror(
            "FATAL: exp length is longer then 4096 characters! Avoiding buffer overrun!" );
        exit( -1 );
    }

    //fill the exp arrays with data
    strcpy( Expression, evalue );
    trim( Expression );

    //case: if exp contains ()
    //fill the expg global arrays with data
    if ( strrchr( Expression, '(' ) != NULL )
    {
        complex = true;
        fprintf( logfile, "COMPLEX expression!\n" );
    }
    else
    {        //05/09/12 reset to false
        complex = false;
        fprintf( logfile, "SIMPLE expression!\n" );
    }

    if ( complex == false )
        fill( Expression );

    if ( complex == true )
    {        //start

        //clearExpGlobals(); //05/09/12 clear the exp globals array

        Exp_Global_Number = tokenize( Expression );

        fprintf( logfile, "exp_gn is:%d\n", Exp_Global_Number );
        //mytrim the exps
        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            trim( expression_Global[i] );
        }

        //start logic
        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            if ( expression_Global[i][0] == '&'
                    || expression_Global[i][0] == '|' )
            {
                plogic[i] = expression_Global[i][0];
                expression_Global[i][0] = ' ';
            }
        }
        //end logic
        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            l = strlen( expression_Global[i] );
            l--;
            if ( expression_Global[i][l] == '&'
                    || expression_Global[i][l] == '|' )
            {
                if ( plogic[i] != '&' && plogic[i] != '|' )
                    plogic[i] = expression_Global[i][l];
                else
                {
                    k = i;
                    k++;
                    fprintf( logfile, "K is:%d\n", k );
                    plogic[k] = expression_Global[i][l];
                }

                expression_Global[i][l] = '\0';
            }
        }

        //mytrim again
        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            trim( expression_Global[i] );
        }
        shuffle( Exp_Global_Number );
        Exp_Global_Number = shuffle2( Exp_Global_Number );
    }        //end of case: complex == true. ()
    return 0;
}

int tuple_populate( char *tuple_str )
{
    int ic = 0;
    while ( tuple_str != NULL )
    {
        sprintf( tuple[ic++], tuple_str );
        tuple_str = strtok( NULL, " " );
    }
    return 0;
}

int get_element(xmlNode * a_node, char *element_name, char operator, char *value)
{
    xmlNode *cur_node = NULL;
    int i, j;
    static int f = 0;
    int numeric_v, num;
    bool string = false;

    for (i=0, j = 0, cur_node = a_node; cur_node; i++, j++, cur_node = cur_node->next)
    {

        if (cur_node->type != 0)
        {
            //fprintf(logfile, "get_element: Element name: %s\n", cur_node->name);
            //fprintf(logfile, "get_element: value: %s\n", cur_node->content);
            if (f == 1 && xmlStrEqual(cur_node->name, (xmlChar *) "text" ) == 1)
            {
                numeric_v = atoi((char *)cur_node->content);
                //fprintf(logfile, "get_element: numeric_v is: %d\n", numeric_v);
                num = atoi(value);

                //test whether the value is a string
                if (isalpha(value[0]) != 0)
                string = true;

                if (operator == '=')
                {
                    if (string != true)
                    {
                        if (numeric_v == num)
                        {
                            fprintf(logfile, "get_element: = FOUND!%s\n", value);
                            f = 0;
                            return(0);
                        }
                        else
                        {
                            f = 0;
                            return(-1);
                        }
                    }
                    if (string == true)
                    {
                        if (strcmp ((char *)cur_node->content, value) == 0)
                        {
                            fprintf(logfile, "get_element: = FOUND!%s\n", value);
                            f = 0;
                            return(0);
                        }
                        else
                        {
                            f = 0;
                            return(-1);
                        }
                    }
                }
                if (operator == '>')
                {
                    if (numeric_v > num)
                    {
                        fprintf(logfile, "get_element: > FOUND!%s\n", value);
                        f = 0;
                        return(0);
                    }
                    else
                    {
                        f = 0;
                        return(-1);
                    }
                }
                if (operator == '<')
                {
                    if (numeric_v < num)
                    {
                        fprintf(logfile, "get_element: < FOUND!%s\n", value);
                        f = 0;
                        return(0);
                    }
                    else
                    {
                        f = 0;
                        return(-1);
                    }
                }
                if (operator == '!')
                {
                    if (string != true)
                    {
                        if (numeric_v != num)
                        {
                            fprintf(logfile, "get_element: ! FOUND!%s\n", value);
                            f = 0;
                            return(0);
                        }
                        else
                        {
                            f = 0;
                            return(-1);
                        }
                    }
                    if (string == true)
                    {
                        if (strcmp ((char *)cur_node->content, value) != 0)
                        {
                            fprintf(logfile, "get_element: ! FOUND!%s\n", value);
                            f = 0;
                            return(0);
                        }
                        else
                        {
                            f = 0;
                            return(-1);
                        }
                    }

                }
                f = 0;
                return(-1);
            }
            if (xmlStrEqual(cur_node->name, (xmlChar *) element_name) == 1)
            {
                //fprintf(logfile, "get_element: Element name: %s\n", cur_node->name);
                fprintf(logfile, "get_element: operator is: %c\n", operator);
                if (f == 0)
                f = 1;
            }
        }
        if (get_element(cur_node->children, element_name, operator, value) == 0)
        {
            return 0;
        }

    }         //end of for loop

    return -1;

}

int get_attribute(xmlNode * a_node, char *attribute_name, char operator, char *value)
{
    xmlNode *cur_node = NULL;
    xmlChar * buf;
    static int f = 0;
    int numeric_v, num;
    bool string = false;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next)
    {

        if (cur_node->type != 0)
        {
            // fprintf(logfile, "get_attribute: Element name: %s\n", cur_node->name);
            // fprintf(logfile, "get_attribute: value: %s\n", cur_node->content);
            /*
             if (f == 1 && xmlStrEqual(cur_node->name, (xmlChar *) "text" ) == 1)
             */
            if (f == 1)
            {
                if (cur_node->properties != NULL)
                {
                    if (cur_node->properties->type == XML_ATTRIBUTE_NODE)
                    {

                        buf = xmlGetProp(cur_node, (xmlChar *) attribute_name);
                        //numeric_v = atoi((char *)cur_node->content);

                        if (buf == NULL)
                        continue;

                        numeric_v = atoi((char *)buf);
                        num = atoi(value);

                        //test whether the value is a string
                        if (isalpha(value[0]) != 0)
                        string = true;

                        if (operator == '=')
                        {
                            if (string != true)
                            {
                                if (numeric_v == num)
                                {
                                    fprintf(logfile, "get_attribute: = FOUND!%s\n", value);
                                    f = 0;
                                    return(0);
                                }
                                else
                                {
                                    f = 0;
                                    return(-1);
                                }
                            }
                            if (string == true)
                            {
                                if (strcmp ((char *)buf, value) == 0)
                                {
                                    fprintf(logfile, "get_attribute: = FOUND!%s\n", value);
                                    f = 0;
                                    return(0);
                                }
                                else
                                {
                                    f = 0;
                                    return(-1);
                                }
                            }
                        }
                        if (operator == '>')
                        {
                            if (numeric_v > num)
                            {
                                fprintf(logfile, "get_attribute: > FOUND!%s\n", value);
                                f = 0;
                                return(0);
                            }
                            else
                            {
                                f = 0;
                                return(-1);
                            }
                        }
                        if (operator == '<')
                        {
                            if (numeric_v < num)
                            {
                                fprintf(logfile, "get_attribute: < FOUND!%s\n", value);
                                f = 0;
                                return(0);
                            }
                            else
                            {
                                f = 0;
                                return(-1);
                            }
                        }
                        if (operator == '!')
                        {
                            if (string != true)
                            {
                                if (numeric_v != num)
                                {
                                    fprintf(logfile, "get_attribute: ! FOUND!%s\n", value);
                                    f = 0;
                                    return(0);
                                }
                                else
                                {
                                    f = 0;
                                    return(-1);
                                }
                            }
                            if (string == true)
                            {
                                if (strcmp ((char *)buf, value) != 0)
                                {
                                    fprintf(logfile, "get_attribute: ! FOUND!%s\n", value);
                                    f = 0;
                                    return(0);
                                }
                                else
                                {
                                    f = 0;
                                    return(-1);
                                }
                            }

                        }
                        f = 0;
                        xmlFree(buf);
                        return(-1);
                    }        //end of type == XML_ATTRIBUTE_NODE
                }        //end of properties != NULL
            }        //end of if (f == 1)

            if (f == 0)
            f = 1;

        }        //end of type != 0
        if (get_attribute(cur_node->children, attribute_name, operator, value) == 0)
        {
            return 0;
        }

    }        //end of for loop
    return -1;
}

int fill( char * exp )
{
    int l = 0;
    int ll = 1;
    int np = 1;
    int i;
    char tmp[256];
    char *tuple_str;

    //we should preserve the original expg[] arrays and work with tmp copy
    strcpy( tmp, exp );

    for ( i = 0; i < strlen( exp ); i++ )
    {
        if ( isspace( exp[i] ) )
            np++;
    }
    fprintf( logfile, "np is:%d\n", np );

    fprintf( logfile, "exp is:%s\n", exp );

    for ( i = 0; i < np; i++ )
    {
        tuple_str = strtok( tmp, " " );
        tuple_populate( tuple_str );
    }

    fprintf( logfile, "tuple_str is:%s\n", tuple_str );

    for ( i = 0; i < np; i++ )
    {
        fprintf( logfile, "%s\n", tuple[i] );
    }

    for ( i = 0; i < np; i += 4, l++, ll++ )
    {
        strcpy( element[l], tuple[i] );
        operator[l] = tuple[i+1][0];
        strcpy( value[l], tuple[i + 2] );
        logic[ll] = tuple[i + 3][0];
    }
    logic[0] = logic[1];
    return 0;
}

int ip_range( char *p1, char *p2, uint32_t pn, uint32_t cn )
{

    int i;
    uint32_t nnetmask;
    struct sockaddr_in sin;
    uint32_t ipa;
    uint32_t ipb;
    uint32_t wildcard_dec;
    uint32_t netmask_dec;

//IPv6 stuff
    bool ipv6 = false;
    int rem, times;
    struct sockaddr_in6 sin6;
    uint32_t ipa6[4];
    uint32_t ipb6[4];
    uint32_t mask[4]; //raw mask
    uint32_t fabmask[4]; //fabricated mask

    if ( strlen( p2 ) <= INET6_ADDRSTRLEN && strchr( p2, ':' ) != NULL )
        ipv6 = true;

    if ( exactflag == 1 )
    {
        if ( pn != cn )
            return -1;
        else nnetmask = cn;
    }
    if ( moreflag == 1 )
    {
        if ( pn <= cn )
            return -1;
        else nnetmask = cn;
    }
    if ( lessflag == 1 )
    {
        if ( pn >= cn )
            return -1;
        else nnetmask = pn;
    }

    fprintf( logfile, "ip_range: prefix netmask:%d\n", pn );
    fprintf( logfile, "ip_range: cmd netmask:%d\n", cn );
    fprintf( logfile, "ip_range: nnetmask:%d\n", nnetmask );

//setting netmask_dec for IPv4
    wildcard_dec = pow( 2, ( 32 - nnetmask ) ) - 1;
    netmask_dec = ~wildcard_dec;

//fprintf(logfile, "wildcard: %ld\n", wildcard_dec);
//fprintf(logfile, "netmask_dec: %ld\n", netmask_dec);

//setting mask array to all zeroes
    for ( i = 0; i < 4; i++ )
        mask[i] = 0;
//setting fab mask array to all zeroes
    for ( i = 0; i < 4; i++ )
        fabmask[i] = 0;

//in case it is IPv6
    if ( ipv6 == true )
    {

//setting remainder & times
        rem = nnetmask % 32;
        times = nnetmask / 32;
        fprintf( logfile, "ip_range: rem is:%d\n", rem );
        fprintf( logfile, "ip_range: times is:%d\n", times );

//setting mask array
        if ( rem == 0 && times > 0 )
        {
            for ( i = 0; i < times; i++ )
                mask[i] = 32;
        }
//setting mask array
        if ( rem > 0 && times > 0 )
        {
            for ( i = 0; i < times; i++ )
                mask[i] = 32;
            mask[i] = rem;
        }

        for ( i = 0; i < 4; i++ )
            fprintf( logfile, "ip_range: mask array is:%d\n", mask[i] );

        if ( inet_pton( AF_INET6, p1, &sin6.sin6_addr ) <= 0 )
        {
            perror( "ip_range: p1:error converting IPv6 address!" );
            return -1;
        }
        else ipv6 = true;

        ipa6[0] = ntohl( sin6.sin6_addr.s6_addr[0] );
        ipa6[1] = ntohl( sin6.sin6_addr.s6_addr[1] );
        ipa6[2] = ntohl( sin6.sin6_addr.s6_addr[2] );
        ipa6[3] = ntohl( sin6.sin6_addr.s6_addr[3] );

        fprintf( logfile, "ip_range: ipa6 0:%d\n", ipa6[0] );
        fprintf( logfile, "ip_range: ipa6 1:%d\n", ipa6[1] );
        fprintf( logfile, "ip_range: ipa6 2:%d\n", ipa6[2] );
        fprintf( logfile, "ip_range: ipa6 3:%d\n", ipa6[3] );

        if ( inet_pton( AF_INET6, p2, &sin6.sin6_addr ) <= 0 )
        {
            perror( "ip_range: p2:error converting IPv6 address!" );
            return -1;
        }
        else ipv6 = true;

        ipb6[0] = ntohl( sin6.sin6_addr.s6_addr[0] );
        ipb6[1] = ntohl( sin6.sin6_addr.s6_addr[1] );
        ipb6[2] = ntohl( sin6.sin6_addr.s6_addr[2] );
        ipb6[3] = ntohl( sin6.sin6_addr.s6_addr[3] );

        fprintf( logfile, "ip_range: ----------------------------------\n" );

        fprintf( logfile, "ip_range: ipb6 0:%d\n", ipb6[0] );
        fprintf( logfile, "ip_range: ipb6 1:%d\n", ipb6[1] );
        fprintf( logfile, "ip_range: ipb6 2:%d\n", ipb6[2] );
        fprintf( logfile, "ip_range: ipb6 3:%d\n", ipb6[3] );

//lets set the bits in the fab mask accordingly
        for ( i = 0; i < 4; i++ )
        {
            wildcard_dec = pow( 2, ( 32 - mask[i] ) ) - 1;
            netmask_dec = ~wildcard_dec;
            fabmask[i] = netmask_dec;
        }

        for ( i = 0; i < 4; i++ )
            fprintf( logfile, "ip_range: fab mask array is:%d\n", fabmask[i] );

        for ( i = 0; i < 4; i++ )
        {
            if ( ( ipa6[i] & fabmask[i] ) == ( ipb6[i] & fabmask[i] ) )
                ;
            else return -1;
        }

        return 0;

    } //end of ipv6 == true

//inet_pton(AF_INET, p1, &sin.sin_addr.s_addr);
//inet_pton(AF_INET, p2, &sin.sin_addr.s_addr);

//in case it is IPv4
    if ( inet_pton( AF_INET, p1, &sin.sin_addr.s_addr ) <= 0 )
    {
        perror( "ip_range: p1:error converting IPv4 address!" );
        return -1;
    }
    else ipa = ntohl( sin.sin_addr.s_addr );

    if ( inet_pton( AF_INET, p2, &sin.sin_addr.s_addr ) <= 0 )
    {
        perror( "ip_range: p2:error converting IPv4 address!" );
        return -1;
    }
    else ipb = ntohl( sin.sin_addr.s_addr );

    fprintf( logfile, "ip_range: p1 content:%s\n", p1 );
    fprintf( logfile, "ip_range: p2 content:%s\n", p2 );

    fprintf( logfile, "ip_range: ------------------\n" );

//*
    uint32_t a = ipa & netmask_dec;
    fprintf( logfile, "ip_range: a: %d\n", a );
    uint32_t b = ipb & netmask_dec;
    fprintf( logfile, "ip_range: b: %d\n", b );
//*/

    if ( ( ipa & netmask_dec ) == ( ipb & netmask_dec ) )
        return 0;
    else return -1;
}

int get_prefix(xmlNode * a_node, char *element_name, char operator, char *value)
{
    xmlNode *cur_node = NULL;
    int xox = -1;
    int zoz = -1;

    xmlChar buf[2*512];
    char str[2*512];
    char ip[INET_ADDRSTRLEN];
    char cidr[INET_ADDRSTRLEN];

    int i, j, z, r;

    int dot = 0;
    int sl = 0;
    uint32_t prefix_netmask;

    uint32_t netmask;
    char tmp[INET6_ADDRSTRLEN];
    char dmp[INET6_ADDRSTRLEN];

//setting operator flag 
    switch (operator)
    {
        case 'e':
        exactflag = 1;
        break;
        case 'l':
        lessflag = 1;
        break;
        case 'm':
        moreflag = 1;
        break;
        default:
        abort ();
    }

    fprintf(logfile, "get_prefix: operator is:%c\n", operator);

//setting netmask
    memset(tmp, '\0', INET6_ADDRSTRLEN);
    memset(dmp, '\0', INET6_ADDRSTRLEN);

    for (i=0; i < strlen (value); i++)
    {
        if (value[i] == '/')
        {
            tmp[0] = value[i+1];
            if (isdigit(value[i+2]))
            tmp[1] = value[i+2];
            //ipv6 check
            if (strchr(value, ':') != NULL)
            if (isdigit(value[i+3]))
            tmp[2] = value[i+3];

            break;
        }
    }

    fprintf(logfile, "get_prefix:tmp is:%s\n", tmp);
    netmask = atol(tmp);
    fprintf(logfile, "get_prefix:netmask:%d\n", netmask);

//setting net
    memset(tmp, '\0', INET6_ADDRSTRLEN);

    for (i=0; i < strlen (value); i++)
    {
        if (value[i] == '/')
        {
            strncpy(tmp, value, i);
            break;
        }
    }

    fprintf(logfile, "get_prefix:tmp is:%s\n", tmp);
    strcpy(dmp, tmp);
    fprintf(logfile, "get_prefix:dmp is:%s\n", dmp);
    fprintf(logfile, "get_prefix:value is:%s\n", value);

//function processing starts here

    for (i=0, j = 0, cur_node = a_node; cur_node; i++, j++, cur_node = cur_node->next)
    {

        if (cur_node->type != 0)
        {
            fprintf(logfile, "get_prefix:node type: Element name: %s\n", cur_node->name);
            fprintf(logfile, "get_prefix:node content: %s\n", cur_node->content);

            if (strcmp(element_name, "PREFIX" ) == 0)
            {
                if (xmlStrEqual(cur_node->name, (xmlChar *) "text" ) == 1 && xmlStrlen(cur_node->content) <= INET6_ADDRSTRLEN )
                {
                    fprintf(logfile, "get_prefix:search () node content: %s\n", cur_node->content);
                    xmlStrPrintf(buf, xmlStrlen(cur_node->content)+1, (char*)cur_node->content);
                    //fprintf(logfile, "xml  buffer is:  %s\n", buf);

                    strcpy (str, (char *) buf);
                    fprintf(logfile, "get_prefix:char str is: %s\n", str);

                    for (z=0; z < strlen (str); z++)
                    if (str[z] == '.')
                    dot++;

//setting net
                    //if (dot == 2)  //08/16/11: it seems the PREFIX format has changed - now we need 3 dots in IP
                    if (dot == 3)
                    { //08/16/11: 3 dots in IP
                        for (z=0; z < strlen (str); z++)
                        {
                            if (str[z] == '/')
                            {
                                sl = 1;
                                fprintf(logfile, "get_prefix:set sl to:%d\n", sl);
                                memset(ip, '\0', INET_ADDRSTRLEN);
                                strncpy(ip, str, z);
                                //strcat(ip, ".0");//08/16/11:it seems the PREFIX format has changed - since 3 dots are in IP - no need to append .0 at the end
                                break;
                            }
                        }
                    }

                    fprintf(logfile, "get_prefix:dot is:%d\n", dot);

//setting netmask
                    memset(cidr, '\0', INET_ADDRSTRLEN);

                    for (z=0; z < strlen (str); z++)
                    {
                        if (str[z] == '/')
                        {
                            cidr[0] = str[z+1];
                            if (isdigit(str[z+2]))
                            cidr[1] = str[z+2];
                            break;
                        }
                    }

                    fprintf(logfile, "get_prefix:cidr is:%s\n", cidr);
                    prefix_netmask = atol(cidr);
                    fprintf(logfile, "get_prefix:prefix_netmask:%d\n", prefix_netmask);

                    if (sl == 1)
                    {
                        fprintf(logfile, "get_prefix:char ip is: %s\n", ip);
                        if ((r = ip_range(ip, dmp, prefix_netmask, netmask)) == 0)
                            fprintf(logfile, "get_prefix:%d: string matched!\n", r);
                        else if (r == -1)
                            fprintf(logfile, "get_prefix:string did not match!\n");

                        if (r == 0)
                        {
                            fprintf(logfile, "get_prefix:%d: inside search() - testing match!\n", 0);
                            return 0;
                        }
                    } //end of sl == 1
                } //end of ->name == text

            } //end of PREFIX == 0

            if (xmlStrEqual(cur_node->name, (xmlChar *) element_name ) == 1)
            if (xox == -1)
            xox=i;
            if (xmlStrEqual(cur_node->content, (xmlChar *) value ) == 1)
            if (zoz == -1)
            zoz=j;

        } //end of ->type != 0

        //    fprintf(logfile, "RETURN!\n");

        if (xox != -1 && zoz != -1)
        {
            fprintf(logfile, "get_prefix: found %s =    %s\n\n", element_name, content);
            return 0;
        }

        if ( get_prefix(cur_node->children, element_name, operator, value ) == 0)
        {
            return 0;
        }
    } //end of for loop
    return -1;
}

// populate expression_Global by breaking up exp into
// multiple strings on '(' and ')' characters
// Return count of the strings populated in expression_Global
int tokenize( char *exp )
{
    int i, h, l, k;
    i = 0;
    h = 0;
    l = 0;
    k = strlen( exp );
    for ( l = 0; l < k; l++ )
    {
        if ( exp[l] != '(' && exp[l] != ')' )
        {
            expression_Global[h][i] = exp[l];
            i++;
        }
        else
        {
            i = 0;
            if ( l != 0 )
                h++;
        }
    }
    return h;
}

int harvest( char *exp, xmlNode *root_element )
{
    int i;
//clear the globals
    for ( i = 0; i < 256; i++ )
    {
        memset( element[i], '\0', sizeof( element[i] ) );
        operator[i] = '\0';
        memset( value[i], '\0', sizeof( value[i] ) );
        logic[i] = '\0';
        memset( tuple[i], '\0', sizeof( tuple[i] ) );
    }
//fill the globals
    fill( exp );
    if ( analyse( root_element ) == 0 )
        return 0;
    else return -1;
}

int shuffle( int exp_gn )
{
    int i, k;
    char exp_tmp[256][MAX_LINE];

    for ( i = 0; i < 256; i++ )
    {
        memset( exp_tmp[i], '\0', sizeof( exp_tmp[i] ) );
    }

    for ( i = 0, k = 0; i < exp_gn; i++ )
    {
        if ( strlen( expression_Global[i] ) != 0 )
        {
            strcpy( exp_tmp[k], expression_Global[i] );
            k++;
        }
    }
    for ( i = 0; i < exp_gn; i++ )
    {
        memset( expression_Global[i], '\0', sizeof( expression_Global[i] ) );
        strcpy( expression_Global[i], exp_tmp[i] );
    }
    return 0;
}

int shuffle2( int exp_gn )
{
    int i, k;
    char tmp[256];

    for ( i = 0; i < 256; i++ )
    {
        tmp[i] = '\0';
    }

    if ( plogic[0] == '\0' && plogic[1] != '\0' )
        plogic[0] = plogic[1];
    if ( plogic[1] == '\0' && plogic[0] != '\0' )
        plogic[1] = plogic[0];

    for ( i = 0, k = 0; i < exp_gn; i++ )
    {
        if ( plogic[i] != '\0' )
        {
            tmp[k] = plogic[i];
            k++;
        }
    }
    for ( i = 0; i < exp_gn; i++ )
    {
        plogic[i] = '\0';
        plogic[i] = tmp[i];
    }
    return k;
}

int getNumLogicalExpressions() {
    int i = 0;
    while ( logic[i] == '&' || logic[i] == '|' )
    {
        i++;
    }

    if ( i == 0 )
        if ( strlen(element[0]) != 0 && operator[0] != 0 && strlen(value[0]) != 0 )
            i = 1;
    return i;
}

//the same as analyze but without writing to a file
//used in complex expression handling
int analyse( xmlNode *root_element )
{
    int exp_n = 0;
    int cycle_and[256];
    int cycle_or[256];
    bool oracle_and = true;
    bool oracle_or = false;
    bool oracle = false;

    int and_count = 0;
    int or_count = 0;

    int i;
//lets fill the logic arrays with zeroes initially
    for ( i = 0; i < 256; i++ )
    {
        cycle_and[i] = 0;
        cycle_or[i] = 0;
    }

//lets count the number of expressions
    exp_n = getNumLogicalExpressions();

    fprintf( logfile, "exp_n is:%d\n", exp_n );

//lets count the AND and OR
    for ( i = 0; i < exp_n; i++ )
    {
        if ( logic[i] == '&' )
            and_count++;
        if ( logic[i] == '|' )
            or_count++;
    }

    for ( i = 0; i < exp_n; i++ ) //loop through defined tags/attributes of the expression
    {
        if ( strcmp( element[i], "PREFIX" ) == 0 )
        { //operate on specified network prefix
            if ( get_prefix(root_element, element[i], operator[i], value[i]) == 0 )
            {
                if ( logic[i] == '&' )
                {
                    cycle_and[i] = 1;
                }
                if ( logic[i] == '|' )
                {
                    cycle_or[i] = 1;
                }
                if ( exp_n == 1 )
                    oracle = true; //if there is just one exp
            }
        }
        else if ( strcmp( element[i], "length" ) == 0
                || strcmp( element[i], "version" ) == 0
                || strcmp( element[i], "type" ) == 0
                || strcmp( element[i], "value" ) == 0
                || strcmp( element[i], "code" ) == 0
                || strcmp( element[i], "timestamp" ) == 0
                || strcmp( element[i], "datetime" ) == 0
                || strcmp( element[i], "precision_time" ) == 0
                || strcmp( element[i], "withdrawn_len" ) == 0
                || strcmp( element[i], "path_attr_len" ) == 0
                || strcmp( element[i], "label" ) == 0 )
        { //operate on specified attributes
            if ( get_attribute(root_element, element[i], operator[i], value[i]) == 0 )
            {
                if ( logic[i] == '&' )
                {
                    cycle_and[i] = 1;
                }
                if ( logic[i] == '|' )
                {
                    cycle_or[i] = 1;
                }
                if ( exp_n == 1 )
                    oracle = true; //if there is just one exp
            }
        }
        else
        {
            if ( get_element(root_element, element[i], operator[i], value[i]) == 0 )
            { //operate on elements
                if ( logic[i] == '&' )
                {
                    cycle_and[i] = 1;
                }
                if ( logic[i] == '|' )
                {
                    cycle_or[i] = 1;
                }
                if ( exp_n == 1 )
                    oracle = true; //if there is just one exp
            }
        }
    } //end of for loop
    fprintf( logfile, "count AND: %d\n", and_count );
    fprintf( logfile, "count OR: %d\n", or_count );

    for ( i = 0; i < and_count; i++ )
        fprintf( logfile, "cycle array AND: %d\n", cycle_and[i] );
//for (i=0; i < or_count; i++)
    for ( i = 0; i < 256; i++ )
        if ( cycle_or[i] == 1 )
            fprintf( logfile, "cycle array OR: %d\n", cycle_or[i] );

//for (i=0; i < and_count; i++)
    for ( i = 0; i < exp_n; i++ )
    {
        if ( cycle_and[i] != 1 )
        {
            oracle_and = false;
            break;
        }
    }
    for ( i = 0; i < exp_n; i++ )
    {
        if ( cycle_or[i] == 1 )
        {
            oracle_or = true;
            break;
        }
    }
    if ( oracle_and == true || oracle_or == true || oracle == true )
    {
        fprintf( logfile, "analyse returned true!\n" );
        return 0;
    }

    return -1;
}

int analyze( const char *filename, xmlNode *root_element, char * buf )
{
    int exp_n = 0;
    int cycle_and[256];
    int cycle_or[256];
    bool oracle_and = true;
    bool oracle_or = false;
    bool oracle = false;

    bool and_found = false;

    int and_count = 0;
    int or_count = 0;

    int i;
    int fd;

    //case when complex = true
    if ( complex == true )
    {
        //section to use after receiving xml chunk

        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            if ( plogic[i] == '&' )
                and_found = true;
        }

        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            if ( harvest( expression_Global[i], root_element ) == 0 )
                truth[i] = 1;
        }

        for ( i = 0; i < Exp_Global_Number; i++ )
            fprintf( logfile, "truth is:%d\n", truth[i] );

        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            if ( plogic[i] == '&' && truth[i] == 1 )
                ;
            else
            {
                if ( plogic[i] == '&' && truth[i] != 1 )
                    Global_AND = false;
                break;
            }
        }

        for ( i = 0; i < Exp_Global_Number; i++ )
        {
            if ( plogic[i] == '|' && truth[i] == 1 )
            {
                Global_OR = true;
                break;
            }
        }
        if ( ( Global_AND == true && and_found == true )
                || ( Global_OR == true ) )
        {
            fprintf( logfile, "analyze: complex exp - g_and is true\n" );
            fd = open( filename, O_CREAT | O_APPEND | O_RDWR, S_IRWU );
            if ( strlen( buf ) != 0 )
                write( fd, buf, strlen( buf ) );
            fprintf( logfile,
                "analyze: complex exp - g_and is true - just about to write into file\n" );
            //put the separator per message
            write( fd, "\n", strlen( "\n" ) );
            fprintf( logfile,
                "analyze: complex exp - g_and is true - wrote into file\n" );

            incMatchingMessages(); //increase the matching count

            //now we put the data in queue as well
            if ( serverflag == 1 )
            {
                if ( buf != NULL )
                {
                    int xlen = strlen( buf );
                    if ( xlen > 64 ) //if xmlR is an actual full BGP message........
                    {
                        fprintf( logfile,
                            "Writer: putting the message into the Queue\n" );
                        //writeQueues((void *)data_ptr->out);
                        writeQueueTable( getQueueTable(), (void *)buf );
                    }
                    else
                    {
                        fprintf( logfile,
                            "Writer: not putting the message into the Queue\n" );
                    }
                }
            }  //end of server flag

            Global_AND = true; //restore to default
            Global_OR = false; //restore to default
            //initialize the parsing engine globals to nulls
            for ( i = 0; i < 256; i++ )
                truth[i] = 0;

            close( fd );   //properly close the file to avoid running out of FDs
            return 0;
            //end of checking oracles
        }
        else
        {
            fprintf( logfile, "g_and or g_or is false\n" );
            //initialize the parsing engine globals to nulls
            for ( i = 0; i < 256; i++ )
                truth[i] = 0;
            return -1;
        }
    } //end of case: complex = true

    //General case when complex = false
    //lets fill the logic arrays with zeroes initially
    for ( i = 0; i < 256; i++ )
    {
        cycle_and[i] = 0;
        cycle_or[i] = 0;
    }

    //lets count the number of expressions
    i = 0;

    while ( logic[i] == '&' || logic[i] == '|' )
    {
        i++;
        exp_n++;
    }

    if ( exp_n == 0 )
        if ( strlen(element[0]) != 0 && operator[0] != 0 && strlen(value[0]) != 0 )
            exp_n = 1;

    fprintf( logfile, "exp_n is:%d\n", exp_n );

    //lets count the AND and OR
    for ( i = 0; i < exp_n; i++ )
    {
        if ( logic[i] == '&' )
            and_count++;
        if ( logic[i] == '|' )
            or_count++;
    }

    //loop through defined tags/attributes of the expression
    for ( i = 0; i < exp_n; i++ )
    {
        if ( strcmp( element[i], "PREFIX" ) == 0 )
        { //operate on specified network prefix
            if ( get_prefix(root_element, element[i], operator[i], value[i]) == 0 )
            {
                if ( logic[i] == '&' )
                {
                    cycle_and[i] = 1;
                }
                if ( logic[i] == '|' )
                {
                    cycle_or[i] = 1;
                }
                if ( exp_n == 1 )
                    oracle = true; //if there is just one exp
            }
        }
        else if ( strcmp( element[i], "length" ) == 0
                || strcmp( element[i], "version" ) == 0
                || strcmp( element[i], "type" ) == 0
                || strcmp( element[i], "value" ) == 0
                || strcmp( element[i], "code" ) == 0
                || strcmp( element[i], "timestamp" ) == 0
                || strcmp( element[i], "datetime" ) == 0
                || strcmp( element[i], "precision_time" ) == 0
                || strcmp( element[i], "withdrawn_len" ) == 0
                || strcmp( element[i], "path_attr_len" ) == 0
                || strcmp( element[i], "label" ) == 0 )
        { //operate on specified attributes
            if ( get_attribute(root_element, element[i], operator[i], value[i]) == 0 )
            {
                if ( logic[i] == '&' )
                {
                    cycle_and[i] = 1;
                }
                if ( logic[i] == '|' )
                {
                    cycle_or[i] = 1;
                }
                if ( exp_n == 1 )
                    oracle = true; //if there is just one exp
            }
        }
        else
        {
            if ( get_element(root_element, element[i], operator[i], value[i]) == 0 )
            { //operate on elements
                if ( logic[i] == '&' )
                {
                    cycle_and[i] = 1;
                }
                if ( logic[i] == '|' )
                {
                    cycle_or[i] = 1;
                }
                if ( exp_n == 1 )
                    oracle = true; //if there is just one exp
            }
        }

        fprintf( logfile, "count AND: %d\n", and_count );
        fprintf( logfile, "count OR: %d\n", or_count );

        for ( i = 0; i < and_count; i++ )
            fprintf( logfile, "cycle array AND: %d\n", cycle_and[i] );
        //for (i=0; i < or_count; i++)
        for ( i = 0; i < 256; i++ )
            if ( cycle_or[i] == 1 )
                fprintf( logfile, "cycle array OR: %d\n", cycle_or[i] );

        //for (i=0; i < and_count; i++)
        for ( i = 0; i < exp_n; i++ )
        {
            if ( cycle_and[i] != 1 )
            {
                oracle_and = false;
                break;
            }
        }
        for ( i = 0; i < exp_n; i++ )
        {
            if ( cycle_or[i] == 1 )
            {
                oracle_or = true;
                break;
            }
        }

        if ( oracle_and == true || oracle_or == true || oracle == true )
        {
            fprintf( logfile,
                "analyze: simple exp - one of the oracles is true\n" );
            fd = open( filename, O_CREAT | O_APPEND | O_RDWR, S_IRWU );
            if ( strlen( buf ) != 0 )
                write( fd, buf, strlen( buf ) );
            //put the separator per message
            write( fd, "\n", strlen( "\n" ) );

            incMatchingMessages(); //increase the matching count

            //now we put the data in queue as well
            if ( serverflag == 1 )
            {
                if ( buf != NULL )
                {
                    int xlen = strlen( buf );
                    if ( xlen > 64 ) //if xmlR is an actual full BGP message........
                    {
                        fprintf( logfile,
                            "Writer: putting the message into the Queue\n" );
                        //writeQueues((void *)data_ptr->out);
                        writeQueueTable( getQueueTable(), (void *)buf );
                    }
                    else
                    {
                        fprintf( logfile,
                            "Writer: not putting the message into the Queue\n" );
                    }
                }
            } //end of server flag

        } //end of checking oracles
        close( fd ); //properly close the file to avoid running out of FDs
        return 0;
    }
    return 0;
}

char * getExpression()
{
    return Expression;
}
