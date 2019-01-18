
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

//This function returns string staring from startIndex for specific substring, i.e. (GET: http://xyz.com\r\n will return http://xyz.com for getSubstring(buffer,"GET",strToReturn,5))
void getsubString(char *buffer,char *subString, char *strToReturn, int startIndex)
{
    char *strToFormat; // This will contain string starting from substring part to end of the string
    strToFormat = strstr(buffer,subString);
    int i=0,j=startIndex;
    while(strToFormat!=NULL && strToFormat[j]!='\r')
    {
      strToReturn[i++] = strToFormat[j++];
    }
}
void getHost(char *buffer,char *subString, char *strToReturn, int startIndex)
{
    char *strToFormat; // This will contain string starting from substring part to end of the string
    strToFormat = strstr(buffer,subString);
    int i=0,j=startIndex;
    while(strToFormat!=NULL && strToFormat[j]!='\r' && strToFormat[j]!=':')
    {
      strToReturn[i++] = strToFormat[j++];
    }
}

//checks if Request is in understandable form or not
int checkForBadRequest(char *input) {
  if (strstr(input, ".") != NULL) {
    return 0;
}
  return -1;
}
//Constructs HTTP response to send to the browser
char* constructHTTPResponse(char *response, char *message) {
  time_t currentTime = time(NULL);
  char *httpResp ,resp[1024]={0};
  char ascDate[50]={0},httpDate[50]={0},contLenStr[50]={0};
  char day[20]={0},month[20]={0}; 
  int date, year, hour, minutes, seconds,contentLen=128;
  strcpy(ascDate,asctime(gmtime(&currentTime)));
  sscanf(ascDate,"%s %s %d %d:%d:%d %d", day, month, &date, &hour, &minutes, &seconds, &year);
  sprintf(httpDate, "%s, %d %s %d %02d:%02d:%02d GMT\r\n",day, date, month, year, hour, minutes, seconds);
  strcat(resp, "HTTP/1.1 ");
  strcat(resp, response);
  strcat(resp, "\r\nDate: ");
  strcat(resp, httpDate);
  strcat(resp, "Accept-Ranges: bytes\r\n");
  strcat(resp, "Content-Length: ");
  contentLen += strlen(response);
  contentLen += strlen(message);
  sprintf(contLenStr,"%d",contentLen);
  strcat(resp, contLenStr);
  strcat(resp, "\r\nContent-Type: text/html; charset=utf-8\r\n\r\n <html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" /></head><body><h2>");
  strcat(resp, response);
  strcat(resp, "</h2><p>");
  strcat(resp, message);
  strcat(resp,"</p></body></html>");
  //printf("%s", resp);
  httpResp = resp;
  return httpResp;
}
//checks for the blocked lists
int checkBlockedSites(char *host,char *filename) {
  int flag = 0;
  FILE *fp;
  char fileBuffer[255];
  fp = fopen(filename, "r");
  if(fp==NULL)
  {
    printf("file name is not correct\n");
    return -1;
  }
  while(fscanf(fp, "%s", fileBuffer) == 1) {
    if(strcmp(host,fileBuffer) == 0) {
      flag = 1;
    }
  }
  return flag;
}
//Sends buffer to the server, receives the response from the server, and received response is sent back to the browser
void doHTTP(char *input, int dataTransferSock, int servSocket)
{
      int ret; char bufferRecvServ[65535]; int bufferLen=65535;
      
      if((ret=send(servSocket,input,strlen(input),0))<0){
        perror("failed while sending information to server");
      }
          
      while(1)
      {  
        memset(bufferRecvServ,0,bufferLen);
        ret=recv(servSocket,bufferRecvServ,bufferLen,0);
        if(ret==0)
          break;
        if(ret<0)
        {
           perror("failed while receiving information from the server");
        }
        if(send(dataTransferSock,bufferRecvServ,ret,0)<0)
        {
          perror("failed while sending information to the browser");
        }                                
      }   
      close((dataTransferSock)); 
      close(servSocket);
}
//Checks for the preffered IP Address
int dns (char *request, int dataTransferSock, char *fileName) {
	    int count=0,sockid_ip,pingSocket=-1;
      char preff_ip[50]={0},ip[20]={0};
      struct addrinfo *index,hints,*result;
      struct sockaddr *sockAddr;
      struct sockaddr_in ipAddress;
      long min_ping, pingtime;
      struct timespec start,finish;
      int flag = -1;
      char *custHttpResp;
      
      memset (&hints, 0, sizeof (hints));
      if(checkBlockedSites(request,fileName) == 1) {
        custHttpResp = constructHTTPResponse("403 Forbidden", "You are not allowed to access the requested resource.");
        if(send(dataTransferSock, custHttpResp, strlen(custHttpResp),0)<0) {
          perror("failed while sending information to the client");
        }
        close((dataTransferSock));
        return -1;
      }
      if(checkBlockedSites(request,fileName) == -1) {
        printf("File could not be found\n");
        return -1;
      }
      //get preferred IP for host name
      getaddrinfo(request, NULL ,&hints,&result); // Return IP addresses for the requested input from client
		  if (result==NULL)
      {
		    custHttpResp = constructHTTPResponse("This site can't be Reached","Could not find the requested server's IP Address.");
        if(send(dataTransferSock, custHttpResp, strlen(custHttpResp),0)<0) {
          perror("failed while sending information to the client");
        }
        close((dataTransferSock));
        return -1;
		  }
      else
      {
        count =0;
			  for(index=result;index!=NULL;index=index->ai_next)
        { // Loop through all the IP addresses returned by the getaddrinfo function call
				  sockAddr = index->ai_addr;         
				  struct sockaddr_in *sockaddrIn = (struct sockaddr_in *)sockAddr; 
				  struct in_addr  *addr = &(sockaddrIn->sin_addr);      
  				inet_ntop(index->ai_family,addr,ip,sizeof(ip)); // Converts the IP address of the requested website to a readable charcter string
				  if((sockid_ip = socket(AF_INET,SOCK_STREAM,0))<0){perror("socket creation for ip failed"); continue;} // Creates a new socket to connect and test the IPs          returned by the getaddrinfo function call
				  ipAddress.sin_family = AF_INET;
				  ipAddress.sin_addr.s_addr = inet_addr(ip);
				  ipAddress.sin_port = htons(80); 
                               
				  if(count==0)
          { // For the first time, preferred IP is the first IP address in the list returned by getaddrinfo() 
				    strcpy(preff_ip,ip);
				  }
				  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start); // The start time of the connection is noted to calculate ping time
				  if(connect(sockid_ip,(struct sockaddr*)&ipAddress,sizeof(ipAddress))==0)
          { // Connects to the IP returned by the getaddrinfo function call
				    clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&finish); // The end time of the connection is noted to calculate ping time
				    pingtime = (long)((finish.tv_nsec)-(start.tv_nsec)); //Ping time is calculated and subsequently the IP with the lowest ping time is selected as the     preferred IP
				    if(count==0)
            { 
				      min_ping=pingtime;
              pingSocket = sockid_ip;
              flag=0;
				    }
				    if(pingtime<min_ping)
            {
						  min_ping = pingtime;
						  strcpy(preff_ip,ip);
              close(pingSocket); 
              pingSocket=sockid_ip;  
              flag = 0;                                                 
					  }
            count++;
            if(flag<0)
            {
              close(sockid_ip);
            }
            
            flag = -1;
          }
        } 
        return pingSocket;      
    }
}
//Receives the buffer from the browser
int doParse(char *buffer, char *host,int sockid,struct sockaddr_in address,int reqCount) {
	
	long ret; int dataTransferSock;
	char url[65535]={0}; char *custHttpResp;
	unsigned int addrlen = sizeof(address);
  char *bufferptr = buffer;
	int inputLen=65535,bufferSize=0;
  
	if((dataTransferSock = accept(sockid,(struct sockaddr*) &address, &addrlen))<0){
	  perror("accept failed");
	  return(-1);
	}
  while(1)
  {
    if((ret=recv(dataTransferSock,bufferptr,inputLen,0))<0){
	    perror("Receive failed");
			return(-1);
     } 
     if(ret==0)
       break;
     bufferSize += ret;
     if(bufferSize>65535){
       custHttpResp = constructHTTPResponse("400 Bad Request", "The request size too large. Closing the connection.");
        if(send(dataTransferSock, custHttpResp, strlen(custHttpResp),0)<0) {
          perror("failed while sending information to the client");
        }
        close((dataTransferSock));
        return -1; 
     }
     memcpy(buffer,bufferptr,ret);  
     getsubString(buffer,"Host",host,6);
     getsubString(buffer,"GET",url,4);  
     if(strlen(url)==0 || strlen(host)==0)
     {
       custHttpResp = constructHTTPResponse("400 Bad Request", "The request size too large. Closing the connection.");
        if(send(dataTransferSock, custHttpResp, strlen(custHttpResp),0)<0) {
          perror("failed while sending information to the client");
        }
        close((dataTransferSock));
       return -1;
     }        
     if((bufferptr[ret-4]=='\r' && bufferptr[ret-3]=='\n' && bufferptr[ret-2]=='\r' && bufferptr[ret-1]=='\n'))
          break;   
     bufferptr += ret;      
  }     
  if(url!=NULL)  
    printf("(%d)   REQ: %s\n",reqCount++, url);
   if(checkForBadRequest(host)) {
     custHttpResp = constructHTTPResponse("400 Bad Request","The request is an invalid domain name.");
     if(send(dataTransferSock, custHttpResp, strlen(custHttpResp),0)<0) {
        perror("failed while sending information to the client");
     }
     close((dataTransferSock));
     return -1;
   }
   memset(url,0,65535); 
   return dataTransferSock;
        
}

int main(int argc,char **argv)
{
  	struct sockaddr_in address;
    int sockid,reqCount=1;
    char fileName[30]={0};
    char buffer[65535]={0};

	  if(argv[1]==NULL || atoi(argv[1])<1024){
		  printf("Please provide valid port number\n");
		  return(-1);
	  }
    if(argv[2]==NULL){
		  printf("Please provide file name for blocked sites\n");
		  return(-1);
	  }
	
	  if((sockid=socket(AF_INET,SOCK_STREAM,0))<0){
		  perror("socket creation failed");
		  return(-1);
	  }
	  strcpy(fileName,argv[2]);
	  address.sin_family = AF_INET;
	  address.sin_port = htons(atoi(argv[1])); 
	  address.sin_addr.s_addr = htonl(INADDR_ANY);
	
	  if(bind(sockid,(struct sockaddr*) &address,(socklen_t) sizeof(address))<0){
		  perror("bind failed");
		  return(-1);
	  }
     
	  if(listen(sockid,5)<0){
		  perror("listen failed ");
		  return(-1);
	  }
	
	  printf("Proxy Server Listening on socket %s\n",argv[1]); 
    while(1)
    {
      char host[100]={0};
	    int dataTransferSock=-1,servSocket=-1;
    
	    if((dataTransferSock=doParse(buffer, host, sockid, address,reqCount))>0)
      {
        servSocket = dns(host, dataTransferSock,fileName);
        reqCount++;
	    }
      if(servSocket>0)
      {
        doHTTP(buffer,dataTransferSock,servSocket);
      }
    }
    close(sockid);
    return 0;
}