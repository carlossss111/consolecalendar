#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sendhttps.h"
#include "jsmn.h"

/*sizes*/
#define NUM_OF_HEADERS 3 /*number of http headers*/
#define CAL_ID_LENGTH 152 /*char size of calendar IDs*/
#define HEADING_WIDTH 80 /*char size of one row of headings in ascii.txt, =0 if you don't want the date centered*/

/*additional constants needed for the authkey request*/
#define SCOPE "calendars.read%20calendars.read.shared%20user.read%20calendars.readwrite%20calendars.readwrite.shared"
#define GRANT_TYPE "refresh_token"

/*colour code macros*/
#define startBold() printf("\033[1m")
#define endBold() printf("\033[0m")

/*absolute file position*/
#define CONFIG_TXT "/usr/local/bin/consolecal_data/config.txt"
#define ASCII_TXT "/usr/local/bin/consolecal_data/ascii.txt"
#define REFRESHKEY_TXT "/usr/local/bin/consolecal_data/refreshkey.txt"

/*
* Choose whether the heading is bold and brighter (0 = no, 1 = yes)
* Choose between heading colours by uncommenting the desired line (and recommenting the original).
*/
   #define isHeadingBold 1
/* #define headingColor() printf("\033[0m") */                       /* default*/
/* #define headingColor() printf("\033[%d;30m",isHeadingBold)   */   /* black  */
/* #define headingColor() printf("\033[%d;31m",isHeadingBold)   */   /* red    */
   #define headingColor() printf("\033[%d;32m",isHeadingBold)        /* green  */
/* #define headingColor() printf("\033[%d;33m",isHeadingBold)   */   /* yellow */
/* #define headingColor() printf("\033[%d;34m",isHeadingBold)   */   /* blue   */
/* #define headingColor() printf("\033[%d;35m",isHeadingBold)   */   /* purple */
/* #define headingColor() printf("\033[%d;36m",isHeadingBold)   */   /* cyan   */
/* #define headingColor() printf("\033[%d;37m",isHeadingBold)   */   /* white  */

/*return the config value searched for*/
/*needs freeing*/
char *getConfig(char *search){
	char *path = CONFIG_TXT, c, *result;
	int i = 0, found = 0;
	long offset;
	FILE *fp;

	/*open file*/
	if(!(fp = fopen(path,"r"))){
		fprintf(stderr, "err ln%d: The config file cannot be opened/read.\n", __LINE__);
		exit(EXIT_FAILURE);
	}

	/*search file for the search token*/
	c = fgetc(fp);
	while(c != EOF){
		if(c == search[i])
			i++;
		else
			i = 0;
		if(i == strlen(search)){
			found = 1;
			break;
		}
		c = fgetc(fp);
	}
	if(!found)
		return "";

	/*hetween the quotemarks, load into memory*/
	while(c != '\"' && c != EOF)
		c = fgetc(fp);
	offset = ftell(fp);
	c = fgetc(fp);
	i = 0;
	while(c != '\"' && c != EOF){
		i++;
		c = fgetc(fp);
	}
	fseek(fp, offset, SEEK_SET);
	result = (char *) malloc(i + 1 * sizeof(char));
	fread(result, sizeof(char), i, fp);
	*(result + i) = '\0';
	fclose(fp);

	return result;
}

/*return today/tomorrow/yesturday etc in YYYY-MM-DD format, needs freeing*/
char *getDay(int timeAdded){
	char *date;
	time_t t = time(NULL);
	struct tm tm;

	/*add time and return date*/
	t += timeAdded;
	tm = *localtime(&t);
	date = (char *) malloc(strlen("YYYY-MM-DD") + 1 * sizeof(char));
	sprintf(date, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	
	return date;
}

/*return provided date in YYYY-MM-DD format, needs freeing*/
char *formatDate(char *argvv){
	char *date;
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	/*light validation using ASCII tables to make sure input is numerical i.e DD/MM*/
	if(
	  argvv[1] < 48   || argvv[1] > 57 ||\
	  argvv[2] != '/' ||\
	  argvv[3] < 48   || argvv[3] > 49 ||\
	  argvv[4] < 48   || argvv[4] > 57  ){
		printf("Argument for date is garbled. Should formatted like 'consolecal DD/MM'\n");
		exit(EXIT_FAILURE);
	}

	date = (char *) malloc(strlen("YYYY-MM-DD") + 1 * sizeof(char));
	sprintf(date, "%04d-%c%c-%c%c", tm.tm_year + 1900, argvv[3], argvv[4], argvv[0], argvv[1]);
	
	return date;
}

/*get day of week using Sakamoto's Method https://shorturl.at/afyLS */
int getDOW(char* str){
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	char strToInt[5];
	int d, m, y;

	/*convert to int, only accepts YYYY-MM-DD*/
	if(strlen(str) != 10)
		return -1;
	sprintf(strToInt, "%.4s", str); y = atoi(strToInt);
	sprintf(strToInt, "%c%c", str[5], str[6]); m = atoi(strToInt);
	sprintf(strToInt, "%c%c", str[8], str[9]); d = atoi(strToInt);

	/*return 0 = sunday, 1 = monday etc*/
    if ( m < 3 )
        y -= 1;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

/*print the day of the week from ascii.txt.*/
void printAsciiArt(int dayOfWeek){
	char *path = ASCII_TXT, c;
	FILE *fp;

	/*validation and file opening*/
	if(dayOfWeek < 0 || dayOfWeek > 6){
		fprintf(stderr,"err ln%d: Day of the week does not exist!\n", __LINE__);
		exit(EXIT_FAILURE);
	}
	if(!(fp = fopen(path,"r")))
		return;/*silent if return if file not found*/

	dayOfWeek += 48;/*to ASCII*/

	/*find correct part of the file*/
	do{
		c = fgetc(fp);
	}while(c != dayOfWeek && c != EOF);
	fgetc(fp); c = fgetc(fp);

	/*print day of week*/
	while(c != dayOfWeek+1 && c != EOF){
		putchar(c);
		c = fgetc(fp);
	}
	fclose(fp);
}

/*structure for grouping JSON properties*/
typedef struct JsonWrapper{
		char *raw;
		int tokenTotal;
		jsmntok_t *tokenPtr; /*nested struct*/
		jsmn_parser parser;  /*nested struct*/
}JsonWrapper;

/*wrapper for jsmn_parse cause why not*/
int jsmnParseWrapper(JsonWrapper *json){
	jsmn_init(&json->parser);

	/*load number of available tokens into json.tokenTotal*/
	if(json->tokenTotal < 0){
		if((json->tokenTotal = jsmn_parse(&json->parser, json->raw, strlen(json->raw), NULL, 0)) < 0){
			fprintf(stderr, "err ln%d: Error code (%d) when parsing JSON UNKNOWN size.\n", __LINE__, json->tokenTotal);
			exit(EXIT_FAILURE);
		}
		return 0;
	}

	/*if json.tokenTotal is known, parse it properly and load the tokens into json.tokenPtr*/
	if((jsmn_parse(&json->parser, json->raw, strlen(json->raw), json->tokenPtr, json->tokenTotal)) < 0){
		fprintf(stderr, "err ln%d: Error code (%d) when parsing JSON of KNOWN size.\n", __LINE__, json->tokenTotal);
		exit(EXIT_FAILURE);
	}
	return 0;
}

/*return whether a given json token is equal to a string*/
int jsoneq(const char *entireJson, jsmntok_t token, const char *comparisonString) {
	if ((int)strlen(comparisonString) == token.end - token.start && \
		strncmp(entireJson + token.start, comparisonString, token.end - token.start) == 0) {
    return 1;
	}
	return 0;
}

/*each key has a matching value right after it. returns the value when given a key and a object to search within*/
/*returns -1 if not found*/
int nextIndexOf(JsonWrapper json, char *searchKey, int objToken){
	int i = objToken;
	int objEnd = json.tokenPtr[objToken].end;

	/*check an object type has been provided*/
	if(json.tokenPtr[objToken].type != JSMN_OBJECT)
		return -1;

	/*navigate to key*/
	while(json.tokenPtr[i].start < objEnd && !jsoneq(json.raw, json.tokenPtr[i], searchKey))
		i++;
	if(json.tokenPtr[i].start >= json.tokenPtr[objToken].end)
		return -1;

	/*print value corresponding to key*/
	return ++i;
}

/*returns token pointer matching the key*/
jsmntok_t nextTokenOf(JsonWrapper json, char *searchKey, int objToken){
	return json.tokenPtr[nextIndexOf(json, searchKey, objToken)];
}

/*given a key and an object token, print the corresponding value*/
void printNext(JsonWrapper json, char *searchKey, int objToken){
	jsmntok_t token = nextTokenOf(json, searchKey, objToken);
	printf("%.*s", token.end - token.start, json.raw + token.start);
}

/*same as printNext but parses escape characters*/
void printNextE(JsonWrapper json, char *searchKey, int objToken){
	jsmntok_t token = nextTokenOf(json, searchKey, objToken);
	char *str, *strStart;
	int size = token.end - token.start;

	str = (char *) malloc(size + 1 * sizeof(char));
	sprintf(str, "%.*s", size, json.raw + token.start);
	strStart = str;

	/*convert '\r\n's to their actual escape sequences, also squash two newlines into one*/
	while(*str != '\0'){
		if((strncmp(str, "\\r\\n\\r\\n", 8) == 0)){
			memset(str, ' ', 7);
			*(str+7) = '\n';
		}
		else if(strncmp(str, "\\r\\n", 4) == 0){
			memset(str, ' ', 3);
			*(str+3) = '\n';
		}
		putchar(*str);
		str++;
	}
	free(strStart);
}

/*given a key and an object token, print part of the value*/
void printNextn(JsonWrapper json, char *searchKey, int objToken, int size, int start){
	printf("%.*s", size, json.raw + nextTokenOf(json, searchKey, objToken).start + start); 
}

/*return number of calendar events from a given JSON*/
/*returns -1 if not found*/
int eventCount(JsonWrapper json){
	int i = 0;

	/*find string matching "value" and stop*/
	i = nextIndexOf(json,"value",0);

	/*the next token (after "value") should be an array of calendar events*/
	if(i != json.tokenTotal && json.tokenPtr[i].type == JSMN_ARRAY)
		return json.tokenPtr[i].size;
	return -1;
}

/*navigates to the object/array after the selected one ends, returns that index*/
/*returns -1 if not found*/
int navigateToNext(JsonWrapper json, int index){
	int end = json.tokenPtr[index].end;
	while(index < json.tokenTotal && json.tokenPtr[index].start < end)
		index++;
	if(index >= json.tokenTotal)
		return -1;
	return index;
}

/*parse the JSON authkey response and return the key only*/
char *parseAuth(char *authResponse){
	char *authKey;
	JsonWrapper *json;
	jsmntok_t authToken;
	json = malloc(sizeof(JsonWrapper));
	json->raw = authResponse;
	json->tokenTotal = -1;
	json->tokenPtr = NULL;

	/*get number of tokens and allocate memory etc*/
	jsmnParseWrapper(json);
	json->tokenPtr = malloc(json->tokenTotal * sizeof(jsmntok_t));
	jsmnParseWrapper(json);

	/*navigate to access token and return it*/
	authToken = nextTokenOf(*json, "access_token", 0);
	authKey = (char *) malloc((authToken.end - authToken.start) + 1 * sizeof(char));
	sprintf(authKey, "%.*s", authToken.end - authToken.start, json->raw + authToken.start);

	/*frees*/
	free(json->tokenPtr);
	free(json);
	free(authResponse);
	return authKey;
}

/*
* Parses a given microsoft calendar,
* displays the events, descriptions and times,
* returns number of events parsed.
*/
int displayCalendar(char *url, char *headers[NUM_OF_HEADERS], char *authkey){
	/*counters*/
	int i;
	int tokenIndex;
	int subTokenIndex;
	int eventTotal = 0;

	/*for json parsing*/
	JsonWrapper *json;
	json = malloc(sizeof(JsonWrapper));
	json->raw = NULL;
	json->tokenTotal = -1;
	json->tokenPtr = NULL;

	/*send HTTP GET request and print response*/
	json->raw = httpsGET(url, NUM_OF_HEADERS, headers);
	#ifdef DEBUG
	printf("RAW_RESPONSE: %s\n", json->raw);
	#endif

	/*get number of tokens for json.tokenTotal*/
	jsmnParseWrapper(json);

	/*allocate memory for the json.tokenPtr and populate it*/
	json->tokenPtr = malloc(json->tokenTotal * sizeof(jsmntok_t));
	jsmnParseWrapper(json); /*the json.tokenPtr is populated here*/

	/*navigate j to list of calendar events*/
	if((tokenIndex = nextIndexOf(*json,"value",0)) < 0){
		fprintf(stderr, "err ln%d: Cannot find list of calendar events.\n", __LINE__);
		exit(EXIT_FAILURE);
	}
	tokenIndex++;

	/*loop through each event*/
	for(i = 0; i < eventCount(*json);i++){
		/*
		* print a calendar event
		*/
		startBold();
		if(jsoneq(json->raw, nextTokenOf(*json, "isAllDay", tokenIndex),"true"))
			printf("   All Day");
		else{
			/*print the HH:MM part of the starting dateTime*/
			subTokenIndex = nextIndexOf(*json, "start", tokenIndex);
			printNextn(*json, "dateTime", subTokenIndex, 5, 11);/*5 characters long, 11th character start*/

			/*print the HH:MMpart of the ending dateTime*/
			subTokenIndex = nextIndexOf(*json, "end", tokenIndex);
			printf(" - ");
			printNextn(*json, "dateTime", subTokenIndex, 5, 11);
		}
		putchar('\n');

		/*print event heading*/
		printf("=============\n");
		printNext(*json, "subject", tokenIndex);
		putchar('\n');
		endBold();

		/*print the event description, if there is any*/
		if(nextTokenOf(*json, "bodyPreview", tokenIndex).start \
		!= nextTokenOf(*json, "bodyPreview", tokenIndex).end){
			printNextE(*json, "bodyPreview", tokenIndex);
			putchar('\n');
		}

		putchar('\n');

		/*go to next calendar event*/
		tokenIndex = navigateToNext(*json,tokenIndex);
	}

	/*end of program*/
	eventTotal = eventCount(*json);

	free(json->raw);
	free(json->tokenPtr);
	free(json);

	return eventTotal;
}

int main(int argc, char** argv){
	/*for calendar requests and auth requests*/
	char *myResponse, *myHeaders[NUM_OF_HEADERS], calendarId[CAL_ID_LENGTH+1];
	char *listCalendarsURL = "https://graph.microsoft.com/v1.0/me/calendars";
	char *authKey, *refreshKey, *authPayload, *clientId, *redirectUri;
	char *authKeyURL = "https://login.microsoftonline.com/organizations/oauth2/v2.0/token";

	char *domain = "https://graph.microsoft.com/v1.0/me/calendars/";
	char *queryPt1 = "/calendarview/?startdatetime=";
	char *queryPt2 = "T00:00:00.000Z&enddatetime=";
	char *queryPt3 = "T23:59:59.999Z";
	char *dateStr, *query, *eventURL;
	int eventTotal = 0;

	/*for json parsing*/
	int i;
	int tokenIndex;
	JsonWrapper *json;
	json = malloc(sizeof(JsonWrapper));
	json->raw = NULL;
	json->tokenTotal = -1;
	json->tokenPtr = NULL;

	/*get formatted date*/
	if(argc < 2 || strncmp("today",argv[1],3) == 0)
		dateStr = getDay(0);
	else if(strncmp("tomorrow",argv[1],3) == 0)
		dateStr = getDay(24*60*60);
	else if(strncmp("yesturday",argv[1],3) == 0)
		dateStr = getDay(24*60*60*-1);
	else if(argv[1][0] > 47 && argv[1][0] < 52)/*see ASCII table*/
		dateStr = formatDate(argv[1]);
	else if(strcmp("help", argv[1]) == 0)
		printf("Please see README.md for usage instructions.\n");
	else{
		printf("Argument not recognised!\n"); 
		return 1;
	}

	/*construct the date with the query string*/
	query = (char *) malloc(strlen(queryPt1) + strlen(dateStr)+ strlen(queryPt2)\
	 + strlen(dateStr) + strlen(queryPt3) + 1 * sizeof(char));
	sprintf(query, "%s%s%s%s%s", queryPt1, dateStr, queryPt2, dateStr, queryPt3);
	
	/*
	* Code must first be obtained from URL once every 90 days.
	* Code then used to obtain refresh-key which should be put into refreshkey.txt manually
	* Refresh key is used (for it's 90 day lifetime) to obtain authkeys below.
	*/
	/*get refresh key*/
	refreshKey = readKey(REFRESHKEY_TXT);

	/*specify headers*/
	myHeaders[0] = "Hostname: login.microsoftonline.com";
	myHeaders[1] = "Content-Type: application/x-www-form-urlencoded";

	/*read configuration file for the clientID and redirectURI*/
	clientId = getConfig("client_id");
	redirectUri = getConfig("redirect_uri");

	/*load the refresh key and the other configs into the http payload*/
	authPayload = (char *) malloc(strlen("client_id=") + strlen(clientId)\
	+ strlen("&scope=") + strlen(SCOPE)\
	+ strlen("&refresh_token=") + strlen(refreshKey)\
	+ strlen("&redirect_uri=") + strlen(redirectUri)\
	+ strlen("&grant_type=") + strlen(GRANT_TYPE) + 1 * sizeof(char));
	sprintf(authPayload, "client_id=%s&scope=%s&refresh_token=%s&redirect_uri=%s&grant_type=%s"\
	, clientId, SCOPE, refreshKey, redirectUri, GRANT_TYPE);

	/*make a POST request for the authkey*/
	authKey = parseAuth( httpsPOST(authKeyURL, 2, myHeaders, authPayload) );

	/*specify headers*/
	myHeaders[0] = "Host: graph.microsoft.com";
	myHeaders[1] = "Prefer: outlook.timezone=\"Europe/London\"";
	myHeaders[2] = (char *) malloc(strlen("Authorization: ") + strlen(authKey) + 1 * sizeof(char));
	sprintf(myHeaders[2],"Authorization: %s",authKey);

	/*GET list of calendars*/
	myResponse = httpsGET(listCalendarsURL, NUM_OF_HEADERS, myHeaders);
	#ifdef DEBUG
	printf("RAW_RESPONSE: %s\n",myResponse);
	#endif

	/*check auth key*/
	if(strstr(myResponse, "InvalidAuthenticationToken") != NULL) {
		fprintf(stderr, "err ln%d: Authkey provided is not valid.\n", __LINE__);
		exit(EXIT_FAILURE);
	}

	/*get number of tokens*/
	json->raw = myResponse;
	jsmnParseWrapper(json);

	/*allocate memory for the json.tokenPtr and populate it*/
	json->tokenPtr = malloc(json->tokenTotal * sizeof(jsmntok_t));
	jsmnParseWrapper(json); /*the json.tokenPtr is populated here*/

	/*navigate token index to list of calendars*/
	if((tokenIndex = nextIndexOf(*json,"value",0)) < 0){
		fprintf(stderr, "err ln%d: Cannot find list of calendars.\n", __LINE__);
		exit(EXIT_FAILURE);
	}
	tokenIndex++;

	/*print day of the week*/
	headingColor();
	printAsciiArt(getDOW(dateStr));
	for(i = 0; i < (HEADING_WIDTH/2)-9; i++)
		putchar(' ');
	startBold();
	printf("~ ~ %c%c/%c%c/%.4s ~ ~\n",  dateStr[8], dateStr[9], dateStr[5], dateStr[6], dateStr);
	endBold();

	/*loop through each calendar*/
	for(i = 0; i < eventCount(*json);i++){
		/*store the calendar id*/
		sprintf(calendarId, "%.*s", CAL_ID_LENGTH, json->raw + nextTokenOf(*json, "id", tokenIndex).start); 

		/*construct the complete calendarview URL*/
		eventURL = (char *) malloc(strlen(domain) + strlen(calendarId) + strlen(query) + 1 * sizeof(char));
		sprintf(eventURL, "%s%s%s", domain, calendarId, query);

		/*display events from given calendar and add to the total*/
		eventTotal += displayCalendar(eventURL, myHeaders, authKey);

		/*free constructed URL and go to next calendar event*/
		tokenIndex = navigateToNext(*json,tokenIndex);
		free(eventURL);
	}

	if(eventTotal == 0)
		printf("This day has no events, enjoy the freedom!\n");
	else
		printf("Found %d total events.\n", eventTotal);

	/*frees*/
	free(clientId);    free(redirectUri);
	free(authPayload); free(refreshKey);
	free(authKey);     free(query);
	free(dateStr);     free(myHeaders[2]);
	free(json->raw);   free(json->tokenPtr);
	free(json);

	return 0;
}