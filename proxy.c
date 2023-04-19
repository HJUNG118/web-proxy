#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/*
캐시 버퍼 구조체 생성
*/
typedef struct cachebuffer{
    char path[MAXLINE]; // 웹 객체의 path
    char data[MAX_OBJECT_SIZE]; // 웹 객체 데이터
    struct cachebuffer* prev; // 이전 항목의 포인터
    struct cachebuffer* next; // 다음 항목의 포인터
    size_t size; // 웹 객체 크기
}cachebuffer;

cachebuffer *cachehead = NULL;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
void doit(int fd);
int parse_uri(char *uri, char *host, char *port, char *path);
void modify_HTTP_header(char *method, char *host, char *port, char *path, int server_fd);
void *thread(void *vargp);
void LRUbuffer();
cachebuffer *find_cache(char *uri);
void add_cache(int object_size, char *from_server_uri, char *from_server_data);
void parse_server(char *buf, char *from_server_uri, char *from_server_data);
int cachesize = 0;

/*
클라이언트와 통신 처리s
*/
void doit(int fd) {
  int server_fd;
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE], path[MAXLINE];
  char host[MAXLINE], port[MAXLINE];
  char server_buf[MAXLINE], from_client_buf[MAXLINE], buf[MAXLINE];
  char from_server_uri[MAXLINE], from_server_data[MAX_OBJECT_SIZE];
  rio_t client_rio, server_rio;
  
  Rio_readinitb(&client_rio, fd); // fd초기화, rio구조체와 연결
  Rio_readlineb(&client_rio, from_client_buf, MAXLINE); // 요청 라인과 헤더를 buf에 읽어들이기
  printf("=====from client=====\n");
  printf("Proxy Request headers:\n");
  sscanf(from_client_buf, "%s %s %s", method, uri, version); // 버퍼에서 HTTP메소드, URI, HTTP 버전 정보를 추출하고 변수에 저장
  printf("%s\n", from_client_buf);
  if (strcasecmp(method,"GET")) { // GET메소드가 아니라면 패스   
        printf("Proxy does not implement the method\n");
        return;
  } 
  
  parse_uri(uri, host, port, path); // 서버의 host, port, path 추출

  char *buffer = find_cache(uri); // 캐시에 요청한 객체가 있는지 확인한다.
  if (buffer != NULL) // 캐시에 클라이언트가 요청한 객체가 있다면
  {
    printf("buffer->data%s\n", buffer);
    Rio_writen(fd, buffer, MAXLINE); // 해당 객체를 클라이언트에 보낸다.
  }
  else // 캐시에 클라이언트가 찾는 객체가 없다면
  {
    server_fd = Open_clientfd(host, port); // proxy와 메인서버 소켓의 파일 디스크립터 생성
    Rio_readinitb(&server_rio, server_fd); // 서버와 연결
    modify_HTTP_header(method, host, port, path, server_fd); // 메인 서버로 보낼 헤더 생성 및 전송
    ssize_t n;
    while ((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) > 0) // 서버로부터 전송된 데이터 읽기
    {
      Rio_writen(fd, server_buf, n); // 실제 읽은 바이트 수(데이터 길이)만큼 데이터 클라이언트로 전송
      cachesize += n;
      if(cachesize < MAX_OBJECT_SIZE)
      {
        strcat(buf, server_buf);
      }    
    }
    printf("cachesize: %d\n", cachesize);
    Close(server_fd);
    // parse_server(buf, from_server_uri, from_server_data); // 서버로부터 받은 uri, 데이터 파싱
    add_cache(strlen(buf), uri, buf); // 버퍼, 버퍼 크기, uri, data
  }
}

/*
서버로부터 받은 응답 uri와 본문으로 파싱
*/
void parse_server(char *buf, char *from_server_uri, char *from_server_data)
{
  char *start_url = strstr(buf, "\r\n");
  strncpy(from_server_uri, buf, start_url - buf);
  strcpy(from_server_data,start_url+2);
  printf("====><><> <%s>\n",from_server_data);
  return;
}

/*
LUR방식으로 캐시에서 제일 오래된 데이터(연결 리스트의 가장 꼬리) 삭제
*/
void LRUbuffer()
{
  printf("===============LRU\n");
  cachebuffer *LRUitem = cachehead; // 새로 생성한 LRUitem노드는 NULL
  while(LRUitem->next != NULL)
  {
    LRUitem = LRUitem->next;
  }
  LRUitem->prev->next = NULL;
  // int size = strlen(LRUitem->data);
  free(LRUitem);
  cachesize -= LRUitem->size; // 캐시에서 빠져나간 객체의 크기만큼 빼준다.
}

/*
클라이언트가 요청한 객체가 캐시에 있는지 확인하고 있다면 응답 데이터 반환, 요청 객체를 가장 최근 리스트로 변경
*/
cachebuffer *find_cache(char *uri)
{
  cachebuffer *currentitem = cachehead;
  while(currentitem != NULL)
  {
    if(strcmp(currentitem->path, uri) == 0)
    {
      if(currentitem->prev != NULL) // 해당 객체가 최근 객체가 아니라면, 최근 리스트로 변경
      {
        printf("같은 객체 하지만 처음은 아닌 찾음\n");
        currentitem->prev->next = currentitem->next;
        if(currentitem->next != NULL)
        {
          currentitem->next->prev = currentitem->prev;
        }
        currentitem->prev = NULL;
        currentitem->next = cachehead;
        cachehead->prev = currentitem;
        cachehead = currentitem;
      }
      printf("같은 객체면서 첫번째 객체 찾음: %d\n", currentitem->data);
      return currentitem->data;
    }
    currentitem = currentitem->next;
  }
  return NULL;
}

/*
웹 서버로부터 받은 객체를 캐시에 저장
*/
void add_cache(int object_size, char *from_server_uri, char *from_server_data)
{
  printf("cachesize: %d\n", cachesize);
  if (cachesize >= MAX_CACHE_SIZE) {
    LRUbuffer();
  }
  printf("=========객체 캐시에 삽입\n");
  cachebuffer *newitem = malloc(sizeof(cachebuffer));
  strcpy(newitem->path, from_server_uri);
  strcpy(newitem->data, from_server_data);
  newitem->size = object_size;
  printf("newitem->size: %d\n", newitem->size);
  newitem->prev = NULL;
  newitem->next = cachehead;
  if (cachehead != NULL) {
    cachehead->prev = newitem;
  }
  cachehead = newitem;
  // cachesize += object_size;
}

/*
메인 서버에게 보낼 헤더 생성 및 전송
*/
void modify_HTTP_header(char *method, char *host, char *port, char *path, int server_fd)
{
  char buf[MAXLINE];

  sprintf(buf, "%s %s %s\r\n", method, path, "HTTP/1.0");
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: %s\r\n", buf, "close");
  sprintf(buf, "%sProxy-Connection: %s\r\n\r\n", buf, "close");
  Rio_writen(server_fd, buf, strlen(buf));
  return;
}

/*
URI를 파싱하여 해당 요청이 정적 콘텐츠인지 동적 콘텐츠인지 판별, 필요한 정보 추출하는 기능을 수행
*/
int parse_uri(char *uri, char *host, char *port, char *path) {
  // http://hostname:port/path
    char *ptr = strstr(uri, "//");
    ptr = ptr != NULL ? ptr + 2 : uri;
    char *host_ptr = ptr;
    char *port_ptr = strchr(ptr, ':');
    char *path_ptr = strchr(ptr, '/');

    if (port_ptr != NULL)
    {
      strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
      strncpy(host, host_ptr, port_ptr - host_ptr);
    }
    else
    {
      strcpy(port, '80');
      strncpy(host, host_ptr, port_ptr - host_ptr);
    }
    strcpy(path, path_ptr);
    return;
}

void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(Pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

/*
클라이언트로부터 요청 대기, 연결 후 통신 처리
*/
int main(int argc, char **argv) {
  // listenfd: 서버 소켓 파일 디스크립터로 클라이언트의 연결 요청을 받아들이는 역할
  // connfd: 클라이언트 소켓 파일 디스크립터로, 클라이언트와의 통신을 위해 사용
  int listenfd, *connfd; 
  pthread_t tid;
  socklen_t clientlen; // 클라이언트 주소 길이 저장하는 변수, 클라이언트 주소 정보를 가져오는데 사용
  struct sockaddr_storage clientaddr; // 클라이언트 주소 정보 저장하는 구조체, 클라이언트와 연결을 허용하는데 사용

  /* Check command line args */
  if (argc != 2) { // 프로그램의 실행 파일명을 포함하여 인수가 2개가 아닌 경우
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법 출력
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 지정된 포트에서 서버 소켓을 열고, 클라이언트 요청 기다린다.
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트와의 연결 수락, 클라이언트의 주소 정보 가져온다.
    Pthread_create(&tid, NULL, thread, connfd);
    // main함수 전체를 무한 반복하여 클라이언트의 연결 요청을 처리
  }
  printf("%s", user_agent_hdr);
  return 0;
}
