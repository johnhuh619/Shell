

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80             /* 명령어의 최대 길이 */
#define READ_END 0				/* fd readEnd */
#define WRITE_END 1				/* fd writeEnd */

/*
 * redirect_stdin - 표준 입력 리다이렉션을 처리한다.
 */
static void redirect_stdin(char *file) {
	/* file을 읽기 전용으로 열고 fd에 파일 디스크립터 저장 */
	int fd = open(file, O_RDONLY);	

	/* 파일 열기에 실패한 경우 에러 메시지를 출력하고 프로그램을 강제 종료한다. */
	if (fd == -1) {				
		perror("open");
		exit(EXIT_FAILURE);		
	}

	/*
	 * 위에서 열었던 파일의 파일 디스크립터(fd)를
	 * 표준 입력 파일 디스크립터(STDIN_FILENO)에 복제하고,
	 * 사용이 끝난 파일 디스크립터(fd)를 닫는다.
	 */
	if (dup2(fd, STDIN_FILENO) == -1) {
		perror("dup2");
		exit(EXIT_FAILURE);
	}
   	close(fd);
}

/*
 * redirect_stdout - 표준 출력 리다이렉션을 처리한다.
 */
static void redirect_stdout(char *file) {
	/* file을 읽고 쓰기 모두 가능하도록 새로 생성 또는 재생성하고 fd에 파일 디스크립터 저장 */
	int fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0644);

	/* 파일 열기에 실패한 경우 에러 메시지를 출력하고 프로그램을 강제 종료한다. */
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

	/*
	 * 위에서 열었던 파일의 파일 디스크립터(fd)를
	 * 표준 출력 파일 디스크립터(STDOUT_FILENO)에 복제하고,
	 * 사용이 끝난 파일 디스크립터(fd)를 닫는다.
	 */
    if (dup2(fd, STDOUT_FILENO) == -1) {
		perror("dup2");
		exit(EXIT_FAILURE);
	}
    close(fd);
}

/*
 * redirect_stdout_append - >>을 통해 기존 파일에 표준 출력을 추가 작성한다.
 */
static void redirect_stdout_append(char *file) {
	/* file을 읽고 쓰기 모두 가능하도록 새로 생성 또는 재생성하고 fd에 파일 디스크립터 저장 */
    int fd = open(file, O_RDWR | O_CREAT | O_APPEND, 0644);

	/* 파일 열기에 실패한 경우 에러 메시지를 출력하고 프로그램을 강제 종료한다. */
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

	/*
	 * 위에서 열었던 파일의 파일 디스크립터(fd)를
	 * 표준 출력 파일 디스크립터(STDOUT_FILENO)에 복제하고,
	 * 사용이 끝난 파일 디스크립터(fd)를 닫는다.
	 */
    if (dup2(fd, STDOUT_FILENO) == -1) {
		perror("dup2");
		exit(EXIT_FAILURE);
	}
    close(fd);
}

/*
 * handle_io_redirection - 표준 입출력 리다이렉션을 관리한다.
 * 명령에 표준 입출력 리다이렉션 기호가 있는지 확인하고
 * 기호가 있는 경우 알맞게 리다이렉션 처리한다.
 * 기호가 없는 경우 아무 일도 일어나지 않는다.
 */
static void handle_io_redirection(char *argv[]) {
	int in_redir_idx = -1;		/* '<' 기호의 인덱스를 저장하는 변수 */
	int out_redir_idx = -1;		/* '>' 기호의 인덱스를 저장하는 변수 */

	/*
 	 * 명령인자 배열에 '<' 기호, '>' 기호, ">>" 기호가 있는지 확인하고 인덱스를 저장한다.
	 * 명령에 같은 리다이렉션 기호가 여러 개 있는 경우 
	 * 가장 먼저 나온 리다이렉션 기호의 인덱스를 저장한다.
 	 */
	for (int i = 0; argv[i] != NULL; i++) {
		if (in_redir_idx != -1 && out_redir_idx != -1) break;
		if (in_redir_idx == -1 && !strcmp(argv[i], "<")) in_redir_idx = i;
		else if (out_redir_idx == -1 && (!strcmp(argv[i], ">") || !strcmp(argv[i], ">>"))) out_redir_idx = i;
	}

	/* 
	 * '<' 기호가 존재하는 경우
	 * 입력 리다이렉션 기호를 명령 인자 배열에서 제거하고,
	 * 파일에서 입력을 받도록 리다이렉션 처리한다.
	 */
	if (in_redir_idx != -1) {
		argv[in_redir_idx] = NULL;
		redirect_stdin(argv[in_redir_idx + 1]);
	}

	/*
	 * '>' 기호 또는 ">>"가 존재하는 경우
	 * 출력 리다이렉션 기호를 명령 인자 배열에서 제거하고,
	 * 파일에 출력을 저장받도록 리다이렉션 처리한다.
	 */
	if (out_redir_idx != -1) {
		if (strcmp(argv[out_redir_idx], ">>") == 0) {
			redirect_stdout_append(argv[out_redir_idx + 1]);
		} else {
			redirect_stdout(argv[out_redir_idx + 1]);
		}
		argv[out_redir_idx] = NULL;
	}
}

/*
 * parse_cmd - 명령어를 파싱한다.
 * 스페이스와 탭을 공백문자로 간주하고, 연속된 공백문자는 하나의 공백문자로 축소한다. 
 * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
 */
static void parse_cmd(char *cmd, char *argv[]) {
    int argc = 0;               /* 인자의 개수 */
    char *p, *q;                /* 명령어를 파싱하기 위한 변수 */

    /*
     * 명령어 앞부분 공백문자를 제거하고 인자를 하나씩 꺼내서 argv에 차례로 저장한다.
     * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
     */

    p = cmd; p += strspn(p, " \t");
    
	do {
        /*
         * 공백문자, 큰 따옴표, 작은 따옴표가 있는지 검사한다.
         */
        q = strpbrk(p, " \t\'\"");
        /*
         * 공백문자가 있거나 아무 것도 없으면 공백문자까지 또는 전체를 하나의 인자로 처리한다.
         */
        if (q == NULL || *q == ' ' || *q == '\t') {
            q = strsep(&p, " \t");
            if (*q) argv[argc++] = q;
        }
        /*
         * 작은 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 작은 따옴표 위치에서 두 번째 작은 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 작은 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else if (*q == '\'') {
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
        }
        /*
         * 큰 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 큰 따옴표 위치에서 두 번째 큰 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 큰 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else if (*q == '\"') {
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
        }
    } while (p);
	argv[argc] = NULL;
}


/*
 * execute_cmd - 명령어를 파싱해서 파이프라인 명령과 표준 입출력 리다이렉션을 고려해서 실행한다.
 */
static void execute_cmd(char *argv[]) {
	int fd[2];					/* 파일 디스크립션 */	
	pid_t pid;					/* 자식 프로세스 아이디 */
	int exec_idx = 0;			/* 실행할 명령의 인덱스를 저장할 변수 */

	/*
	 * 명령인자의 끝에 도달할 때까지 파이프 기호가 있는지 검사하고,
	 * 파이프 기호가 있으면 자식 프로세스를 생성하여 앞의 명령어를 처리하고 실행한다.
	 */
	for (int i = 0; argv[i] != NULL; i++) {
		/* 파이프 기호가 나올 때까지 반복한다. */ 
		if (strcmp(argv[i], "|")) continue;

		/* 파이프를 생성한다. */
		if (pipe(fd) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		/* 자식 프로세스를 생성한다. */
		if ((pid = fork()) == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		/*
		 * 자식 프로세스는 파이프 기호 앞의 명령어를 표준 입출력을 처리한 후 실행한다.
		 * 사용하지 않을 파이프의 읽기 종단의 파일 디스크립터(fd[READ_END])을 닫아주고,
		 * 표준 출력의 파일 디스크립터에 파이프의 쓰기 종단의 파일 디스크립터를 복제한다.
		 * 이후 사용이 끝난 파이프의 쓰기 종단의 파일 디스크립터를 닫아주고,
		 * 명령어 실행을 위해 명령 인자 배열에서 파이프 기호를 제거하고,
		 * 리다이렉션 처리를 해준 후 명령을 실행한다.
		 */
		if (pid == 0) {
			close(fd[READ_END]);
			if (dup2(fd[WRITE_END], STDOUT_FILENO) == -1) {
				perror("dup2");
				exit(EXIT_FAILURE);
			}
			close(fd[WRITE_END]);
			argv[i] = NULL;
			handle_io_redirection(&argv[exec_idx]);
			if(execvp(argv[exec_idx], &argv[exec_idx]) == -1){
				printf("Command Not Found | Invalid Syntax\n");
				exit(EXIT_FAILURE);
			}
		} 

		/*
		 * 부모 프로세스는 사용하지 않을 파일 디스크립터(fd[WRITE_END])을 닫아주고,
		 * 표준 입력의 파일 디스크립터(STDIN_FILENO)에
		 * 파이프의 읽기 종단(fd[READ_END])을 복제한다.
		 * 이후 사용이 끝난 파일 디스크립터(fd[READ_END])을 닫아주고,
		 * 실행할 명령 인자의 인덱스 값을 파이프 기호 뒤의 명령 인자로 변경해준다.
		 */
		else {
			close(fd[WRITE_END]);
			if (dup2(fd[READ_END], STDIN_FILENO) == -1) {
				perror("dup2");
				exit(EXIT_FAILURE);
			}
			close(fd[READ_END]);
			exec_idx = i + 1;
		}
	}
	/* 마지막 남은 명령 하나를 실행한다. */
	handle_io_redirection(&argv[exec_idx]);
	if(execvp(argv[exec_idx], &argv[exec_idx])== -1){
		printf("Command Not Found | Invalid Syndatx\n");
		exit(EXIT_FAILURE);
	}
}

/*
 * 작은 쉘 프로그램이다.
 * 프로그램에서 지원하는 기능: 
 * 1. 파이프라인 명령
 * 2. 표준 입출력 리다이렉션
 * 3. 백그라운드 실행
 * 4. '/' 입력후 줄 바꿔 명령어 입력 가능
 * 5. 현재 작업중인 디렉토리 상시 확인 가능
 */
int main(void) {
    char cmd[MAX_LINE+1];       /* 명령어를 저장하기 위한 버퍼 */
    int len;                    /* 입력된 명령어의 길이 */
    pid_t pid;                  /* 자식 프로세스 아이디 */
    int background;             /* 백그라운드 실행 유무 */ 
    char *argv[MAX_LINE/2+1];   /* 명령어 인자를 저장하기 위한 배열 */

    /*
     * 종료 명령인 "exit"이 입력될 때까지 루프를 무한 반복한다.
     */
    while (true) {
        /*
         * 좀비 (자식)프로세스가 있으면 제거한다.
         */
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);

		/* 현재 작업 디렉토리의 위치를 출력한다 */
		char cwd[1024];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
        	fprintf(stderr, "Failed to get current working directory\n");
			exit(EXIT_FAILURE);
    	}
		printf("%s", cwd);


        /*
         * 셸 프롬프트를 출력한다. 지연 출력을 방지하기 위해 출력버퍼를 강제로 비운다.
         */

        printf(" shell> "); fflush(stdout);
        /*
         * 표준 입력장치로부터 최대 MAX_LINE까지 명령어를 입력 받는다.
         * 입력된 명령어 끝에 있는 새줄문자를 널문자로 바꿔 C 문자열로 만든다.
         * 입력된 값이 없으면 새 명령어를 받기 위해 루프의 처음으로 간다.
         */

		if (fgets(cmd, MAX_LINE, stdin) == NULL) {
			if (feof(stdin)) {
				printf("stdin success");
				exit(EXIT_SUCCESS);
			}
			else {
				perror("fgets");
				exit(EXIT_FAILURE);
			}
		}
		
		/*
		 * 줄바꿈 문자를 널문자로 변경하는 로직.
		 * 명령어 입력 도중 \을 통해 줄바꿈 후 추가 입력을 받을 수 있도록 한다.
		 * fgets()는 문자열에 '\n', '\0' 를 모두 포함한다는 특성 고려.
		 */
		char *pos;
		int current_len;
		bool needMoreInput = false;

		pos = strchr(cmd, '\n');
		if(pos != NULL && pos > cmd && *(pos-1) == '\\'){
			needMoreInput = true;
			*(pos -1) = '\0';
		}
		while(needMoreInput){
			fflush(stdout);
			if(fgets(pos,MAX_LINE - strlen(cmd), stdin) == NULL){
				perror("fgets");
				exit(EXIT_FAILURE);
			}
			needMoreInput = false;
			pos = strchr(cmd, '\n');
			if(pos != NULL && pos > cmd && *(pos -1) == '\\'){
				needMoreInput = true;
				*(pos - 1) = '\0';
			}
		}



		/*
		 * 빈 명령이라면 루프 다시 시작.
		 */
		if (strlen(cmd) == 0) {
			goto start_loop;
		}
		
  
        /*
         * 종료 명령이면 루프를 빠져나간다.
         */
        if(!strcasecmp(cmd, "exit"))
            break;

        
		/*
		 * 백그라운드 명령인지 확인하고, '&' 기호를 삭제한다.
		 */
		char* token = strtok(cmd, "\n");

		while (token != NULL) {
			if (strcmp(token, "exit") == 0) {
				exit(EXIT_SUCCESS);
			}
			
			if (token[strlen(token) - 1] == '&') {
				printf("It's background pocess\n");
				background = 1;
				token[strlen(token) - 1] = '\0';
			}
			else
				background = 0;
			token = strtok(NULL, "\n");
		}
	
		/*
		 * cmd를 명령인자배열 argv에 파싱하여 저장하고,
		 * 명령어가 cd인지 확인하고 cd라면 현재 작업 디렉토리를 변경한다.
		 */

		parse_cmd(cmd, argv);
		if (strcmp(argv[0], "cd") == 0) {
			if (chdir(argv[1]) == -1) {
				perror("cd");
				exit(EXIT_FAILURE);
			}
			continue;
		}
	
        /*
         * 자식 프로세스를 생성하여 입력된 명령어를 실행하게 한다.
         */
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         */
        else if (pid == 0) {
            execute_cmd(argv);
            exit(EXIT_SUCCESS);
        }
        /*
         * 포그라운드 실행이면 부모 프로세스는 자식이 끝날 때까지 기다린다.
         * 백그라운드 실행이면 기다리지 않고 다음 명령어를 입력받기 위해 루프의 처음으로 간다.
         */
        else if (!background)
            waitpid(pid, NULL, 0);
	start_loop:
		continue;
    }
    return 0;
}
