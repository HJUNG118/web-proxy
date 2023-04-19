/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *uri);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *uri);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/*
클라이언트와의 통신 처리 함수
*/
void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE]; // filename: 정적인 파일 경로 저장하는 버퍼, cgiargs: CGI 프로그램 인자 값 저장하는 버퍼
  rio_t rio;
  
  /* Read request line and headers */
  Rio_readinitb(&rio, fd); // 동기화
  Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인과 헤더를 buf에 읽어들이기
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // 버퍼에서 HTTP메소드, URI, HTTP 버전 정보를 추출하고 변수에 저장
  if (strcasecmp(method, "GET")) { // HTTP메소드가 GET인지 확인, GET이 아니라면 
    // clienterror() 함수 호출하여 에러 응답 생성
    clienterror(fd, method, "501", "Not implemented","Tiny does not implement this method");
    return; 
  }
  
  read_requesthdrs(&rio); // 요청 헤더를 읽어드린다.
  
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // URI 파싱하여 filename, cgiargs 변수에 파일 이름과 CGI인자 저장, 콘텐츠가 정적인지 동적인지 확인
  if (stat(filename, &sbuf) < 0) { // filename에 해당하는 파일의 정보 가져오기, 존재하지 않는 경우 404에러 응답 생성
      clienterror(fd, filename, "404", "Not found","Tiny couldn't find this file");
      return; 
  }

  if (is_static) { // 파일이 정적 콘텐츠일 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 파일이 일반 파일이고, 사용자에게 읽기 권한이 있는지 확인
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file"); // 없다면 403에러 응답 생성
        return; 
    }
    serve_static(fd, filename, sbuf.st_size, uri);
  }
  else { 
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
          clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
          return; 
      }
      serve_dynamic(fd, filename, cgiargs, uri); // 동적으로 CGI 프로그램 실행, 결과를 클라이언트에게 전송
  }
}

/*
요청 헤더를 읽어들이는 함수
*/
void read_requesthdrs(rio_t *rp) { // 소켓으로부터 읽어들인 데이터를 처리하기 위한 구조체 변수를 매개변수로 받기
    char buf[MAXLINE]; // 최대 길이 MAXLINE만큼 데이터 읽어들이기
    Rio_readlineb(rp, buf, MAXLINE); // rp로부터 한 줄씩 데이터 읽어들이기

    while(strcmp(buf, "\r\n")) { // 버퍼에 저장된 데이터가 '\r\n'과 같지 않은 동안 HTTP 요청 헤더의 끝에 도달전까지 반복
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
URI를 파싱하여 해당 요청이 정적 콘텐츠인지 동적 콘텐츠인지 판별, 필요한 정보 추출하는 기능을 수행하는 함수
*/
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    if (!strstr(uri, "cgi-bin")) {  // URI 문자열에 cgi-bin이라는 문자열이 포함되어 있지 않다면 즉, 정적 콘텐츠라면
        strcpy(cgiargs, ""); // cgiargs 버퍼 빈 문자열로 초기화
        strcpy(filename, "."); // filename 버퍼에 현재 디렉토리(".")와 uri를 연결하여 정적 컨텐츠의 파일 경로 생성
        strcat(filename, uri); 
        if (uri[strlen(uri)-1] == '/') { // uri 마지막 문자가 '/'인 경우 즉, 디렉토리를 요청한 경우 filename 버퍼에 home.html추가
            strcat(filename, "home.html");
        }
        return 1; // return 1로 정적 컨텐츠를 요청한 것임을 반환
    }
    else {  // 동적 콘텐츠를 요청한 경우라면
        ptr = index(uri, '?'); // uri에서 '?'문자 찾기
        if (ptr) { // '?'문자가 포함되었다면 
            strcpy(cgiargs, ptr+1); // cgiargs 버퍼에 '?'문자 다음 문자열을 복사
            *ptr = '\0'; // '?'를 '\0'으로 대체
        }
        else {
            strcpy(cgiargs, ""); // cgiargs 버퍼 빈 문자열 초기화
        }
        strcpy(filename, "."); // filename 버퍼에 현재 디렉토리와 uri를 연결하여 동적 컨텐츠 파일 경로 생성
        strcat(filename, uri);
        return 0;
    }
}

/*
정적인 파일을 클라이언트에게 전송하는 역할을 하는 함수
*/
void serve_static(int fd, char *filename, int filesize, char *uri) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF], *usrbuf;

    /* Send response headers to client */
    get_filetype(filename, filetype); // 파일 타입 가져오기
    sprintf(buf, "%s\r\n", filename+1);
    sprintf(buf, "%sHTTP/1.0 200 OK\r\n", buf); // 응답해더를 생성해서 buf에 저장
    // sprintf(buf, "filename: %s\r\n", filename);
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf)); // 버퍼에 저장된 HTTP 응답 헤더를 클라이언트에게 전송
    printf("Response headers:\n"); 
    printf("%s", buf);

    /* filename에 해당하는 파일을 읽어 'srcfd' 파일 디스크립터를 통해 열고,
    'srcp' 포인터를 통해 메모리 매핑 수행 */
    srcfd = Open(filename, O_RDONLY, 0); 
    usrbuf = (char *)malloc(filesize);
    Rio_readn(srcfd, usrbuf, filesize); // srcfd: 데이터를 읽어올 fd, usrbuf: 읽어온 데이터 저장할 버퍼, 버퍼크기
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, usrbuf, filesize); // scrp를 클라이언트에게 전송하여 파일의 내용을 응답 본문으로 전송
    // Munmap(srcp, filesize); // 매핑된 srcp를 해제
    free(usrbuf);
}

/*
파일 이름에 따라 해당 파일의 MIME 타입(html, pdf 등)을 결정하는 함수
*/
void get_filetype(char *filename, char *filetype) { // 파일 확장자를 검사하여 MIME 타입을 결정
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else // 확장자가 없을 경우 planin MIME 타입을 설정
        strcpy(filetype, "text/plain");
}

/*
클라이언트로부터 받은 요청에 따라 CGI 프로그램을 실행하고,
그 결과를 클라이언트에게 전송하여 동적인 콘텐츠를 제공하는 기능 수행
*/
void serve_dynamic(int fd, char *filename, char *cgiargs, char *uri) {
    char buf[MAXLINE], *emptylist[] = { NULL };
    /* Return first part of HTTP response */
    sprintf(buf, "%s\r\n", filename+1);
    sprintf(buf, "%sHTTP/1.0 200 OK\r\n", buf);
    Rio_writen(fd, buf, strlen(buf)); // 파일 디스크립터 fd에 buf의 내용 전송
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    if (Fork() == 0) { // 자식 프로세스 생성하고, 자식 프로세스에서 아래 작업 수행
        setenv("QUERY_STRING", cgiargs, 1); // CGI 프로그램에게 클라이언트로부터 전달된 쿼리 스트링 데이터 전달
        Dup2(fd, STDOUT_FILENO); // 파일 디스크립터 fd를 표준 출력으로 복제
        Execve(filename, emptylist, environ); // filename에 지정된 CGI 프로그램 실행
    }
    Wait(NULL); // CGI 프로그램의 실행이 완료될 때까지 대기
}

/*
HTTP 응답 메시지 생성, 클라이언트에게 전송하는 역할 수행
*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  // buf: HTTP 요청 및 응답 헤더 저장
  // body: HTTP 요청 및 응답의 본문 데이터 저장
  char buf[MAXLINE], body[MAXBUF]; 
  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

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

  listenfd = Open_listenfd(argv[1]); // 지정된 포트에서 서버 소켓을 열고, 클라이언트의 요청을 기다린다.
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트와의 연결을 수락, 클라이언트의 주소 정보를 가져온다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 주소를 호스트 이름과 포트로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // 클라이언트와의 통신 처리
    Close(connfd);  // 연결 종료
    // main함수 전체를 무한 반복하여 클라이언트의 연결 요청을 처리한다.
  }
}