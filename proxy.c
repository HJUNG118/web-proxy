#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
void doit(int fd);
int parse_uri(char *uri, char *host, char *port, char *path);
void modify_HTTP_header(char *method, char *host, char *port, char *path, int server_fd);

/*
클라이언트와 통신 처리
*/
void doit(int fd) {
  int server_fd;
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE], path[MAXLINE];
  char host[MAXLINE], port[MAXLINE];
  char server_buf[MAXLINE], from_client_buf[MAXLINE];
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
  server_fd = Open_clientfd(host, port); // proxy와 메인서버 소켓의 파일 디스크립터 생성

  Rio_readinitb(&server_rio, server_fd);
  modify_HTTP_header(method, host, port, path, server_fd); // 메인 서버로 보낼 헤더 생성 및 전송
  ssize_t n;
  while ((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) > 0) // 서버로부터 전송된 데이터 읽기
  {
      Rio_writen(fd, server_buf, n); // 실제 읽은 바이트 수(데이터 길이)만큼만 쓰도록 수정
      // 클라이언트로 다시 데이터 전송
  }
  Close(server_fd);
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

/*
클라이언트로부터 요청 대기, 연결 후 통신 처리
*/
int main(int argc, char **argv) {
  // listenfd: 서버 소켓 파일 디스크립터로 클라이언트의 연결 요청을 받아들이는 역할
  // connfd: 클라이언트 소켓 파일 디스크립터로, 클라이언트와의 통신을 위해 사용
  int listenfd, connfd; 
  char hostname[MAXLINE], port[MAXLINE];
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
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트와의 연결 수락, 클라이언트의 주소 정보 가져온다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 주소를 호스트 이름과 포트로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // 클라이언트와의 통신 처리
    Close(connfd);  // 연결 종료
    // main함수 전체를 무한 반복하여 클라이언트의 연결 요청을 처리
  }
  printf("%s", user_agent_hdr);
  return 0;
}
