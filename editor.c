#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define GREEN 			"\033[1;32m"
#define BLUE 			"\033[1;34m"
#define DEFAULT_COLOR 	"\033[0;m"

void color(char* c){
	write(STDOUT_FILENO, c, strlen(c));
}

void affiche_cmd(char* argv[]){
    int i = 0;
    while(argv[i] != NULL){
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));
        if(argv[i+1] != NULL) write(STDOUT_FILENO, " ", 1);
        i++;
    }
    if(argv[0] != NULL) write(STDOUT_FILENO, "\n", 1);
}

void affiche_cmd_pipes(char** argv[]){
	for(int i = 0; argv[i] != NULL; i++){
		affiche_cmd(argv[i]);
	}
}

void* myalloc(size_t sz){
	void* alloc = malloc(sz);
	if(alloc == NULL) exit(EXIT_FAILURE);
	return alloc;
}

void free_argv_pipes(char*** argv[]){
	for(int i = 0; (*argv)[i] != NULL; i++){
		for(int j = 0; (*argv)[i][j] != NULL; j++){
			free((*argv)[i][j]);
		}
		free((*argv)[i]);
	}
	free(*argv);
}

int parse_line(char *s, char **argv[]){
	if(s == NULL) return 1;
	int i = 0;
	if(!strlen(s)){
		(*argv)[0] = NULL;
		return 1;
	}else{
		if(strpbrk(s, "=") != NULL){
			char name[1024], value[1024];
			char* env = strtok(s, "=");
			strcpy(name, env);
			env = strtok(NULL, "=");
			strcpy(value, env);
			setenv(name, value, 1);
		}else if(strpbrk(s, "$") != NULL){
			char name[1024];
			strcpy(name, strpbrk(s, "$")+1);
			(*argv)[0] = myalloc(sizeof(char)*(strlen(getenv(name))+1));
			strcpy((*argv)[0], getenv(name));
			i++;
		}else{
			char* buffer = strtok(s, " ");
			while(buffer != NULL && buffer[0] == '#') buffer = strtok(NULL, " ");
			if(buffer == NULL) { (*argv)[0] = NULL; return 1; }
			(*argv)[i] = myalloc(sizeof(char)*(strlen(buffer)+1));
			strcpy((*argv)[i++], buffer);
			while((buffer = strtok(NULL, " ")) != NULL){
				while(buffer != NULL && buffer[0] == '#') buffer = strtok(NULL, " ");
				if(buffer == NULL) { (*argv)[0] = NULL; return 1; }
				(*argv)[i] = myalloc(sizeof(char)*(strlen(buffer)+1));
				strcpy((*argv)[i++], buffer);
			}
		}
		(*argv)[i] = NULL;
		return 0;
	}
}

void simple_cmd(char *argv[]){
	if(argv[0] == NULL) return;
	if(!strcmp(argv[0], "exit")) {
		for(int i = 0; argv[i] != NULL; i++)
			free(argv[i]);
		free(argv);	
		exit(EXIT_SUCCESS);
	}
	else if(!strcmp(argv[0], "cd")){
		if(argv[1] != NULL){
			chdir(argv[1]);
		}
	}else if(fork() == 0){
		execvp(argv[0], argv);
	}else{
		wait(NULL);
	}
}

void affiche_rep(){
	char buffer[1024] = {0};
	getcwd(buffer, 1024);
	int size = strlen(buffer);
	buffer[size] = '\0';
	color(BLUE);
	write(STDOUT_FILENO, buffer, size);
	color(DEFAULT_COLOR);
	write(STDOUT_FILENO, "$ ", 2);
}

void parse_line_redir(char *s, char **argv [], char **in , char **out){
 	parse_line(s, argv);
 	int i=0, entree = 0, sortie = 0;
 	while((*argv)[i]!=NULL && (*argv)[i+1]!=NULL){
 		if(!strcmp((*argv)[i], "<")){
 			*in = malloc(strlen((*argv)[i+1])+1);
 			strcpy(*in, (*argv)[i+1]);
			entree = 1;
			(*argv)[i] = NULL;
 		}
 		else if(!strcmp((*argv)[i], ">")){
 			*out = malloc(strlen((*argv)[i+1])+1);
 			strcpy(*out, (*argv)[i+1]);
			sortie = 1;
			(*argv)[i] = NULL;
 		}
 		i++;
 	}
	if(!entree && !sortie){
		*in = NULL;
		*out = NULL;
	}else if(!entree){
		*in = NULL;
	}else if(!sortie){
		*out = NULL;
	}
}

int redir_cmd(char* argv[], char* in, char* out){
	int stdout, stdin;
	if(out != NULL){
		stdout = dup(STDOUT_FILENO);
		int fdout = open(out, O_WRONLY | O_CREAT, 00755);
		if(fdout == -1) exit(EXIT_FAILURE);
		dup2(fdout, STDOUT_FILENO);
		close(fdout);
	}
	if(in != NULL){
		stdin = dup(STDIN_FILENO);
		int fdin = open(in, O_RDONLY);
		if(fdin == -1) exit(EXIT_FAILURE);
		dup2(fdin, STDIN_FILENO);
		close(fdin);
	}
	simple_cmd(argv);
	if(out != NULL) dup2(stdout, STDOUT_FILENO);
	if(in != NULL) dup2(stdin, STDIN_FILENO);
	if(out != NULL) close(stdin);
	if(in != NULL) close(stdout);
	return 0;
}

void parse_line_pipes(char *s, char ***argv[], char **in, char **out){
	parse_line_redir(s, *argv, in, out);
	int i = 0, k = 0;
	while((**argv)[i] != NULL || (**argv)[i+1] != NULL){
		if((**argv)[i] != NULL && !strcmp((**argv)[i], "|")){
			(**argv)[i] = NULL;
			(*argv)[k+1] = (**argv+i+1);
			k++;
		}
		i++;
	}
	(*argv)[k+1] = NULL;
}

int redir_cmd_pipes(char** argv[], char* in, char* out){
	int fd[2], buff, i;
	int stdout = dup(STDOUT_FILENO);
	int stdin = dup(STDIN_FILENO);
	for(i = 0; argv[i] != NULL; i++){
		pipe(fd);
		if(i != 0) {
			dup2(buff, STDIN_FILENO);
			close(buff);
		}
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		if(argv[i+1] != NULL) 
			simple_cmd(argv[i]);
		buff = dup(fd[0]);
	}
	dup2(stdout, STDOUT_FILENO);
	redir_cmd(argv[i-1], in, out);
	dup2(stdin, STDIN_FILENO);
	free_argv_pipes(&argv);
	close(stdin);
	close(stdout);
	return 0;
}

void exec_cmd(char*** argv){
	char string[1024] = {0};
	char* in;
	char* out;
	ssize_t r = read(STDIN_FILENO, string, 1024);
	if(r > 0) string[r-1] = '\0';
	if(r == 0){ 
		write(STDOUT_FILENO, "\n", 1);
		exit(EXIT_FAILURE);
	}
	parse_line_pipes(string, &argv, &in, &out);
	redir_cmd_pipes(argv, in, out);
}

void launch();

void ctrlz(){
	write(STDOUT_FILENO, "\n", 1);
	affiche_rep();
}

void launch(){
	while(1){
		struct sigaction sg;
		sigemptyset(&sg.sa_mask);
		sg.sa_handler = ctrlz;
		sg.sa_flags = SA_RESTART;
		sigaction(SIGTSTP, &sg, NULL);
		char*** argv = myalloc(sizeof(char**) * 10);
		*argv = myalloc(sizeof(char*) * 100);
		for(int i = 0; i < 100; i++)
			(*argv)[i] = NULL;
		affiche_rep();
		exec_cmd(argv);
	}
}

int main(int argc, char* argv[])
{
	if(argc == 1) launch();
	else{
		char *saveptr, *in, *out;
		char* ex = strpbrk(argv[1], ".");
		if(ex == NULL) launch();
		if(strcmp(ex, ".sh")) launch();
		int fd = open(argv[1], O_RDONLY);
		if(fd == -1) exit(EXIT_FAILURE);
		char buffer[1024] = {0};
		while(read(fd, buffer, 1024) != 0){
			char*** argv2 = myalloc(sizeof(char**) * 10);
			*argv2 = myalloc(sizeof(char*) * 100);
			char* string = strtok_r(buffer, "\n", &saveptr);
			parse_line_pipes(string, &argv2, &in, &out);
			redir_cmd_pipes(argv2, in, out);
			while((string = strtok_r(NULL, "\n", &saveptr)) != NULL){
				char*** argv2 = myalloc(sizeof(char**) * 10);
				*argv2 = myalloc(sizeof(char*) * 100);
				parse_line_pipes(string, &argv2, &in, &out);
				redir_cmd_pipes(argv2, in, out);
			}
		}
		close(fd);
	}
	return 0;
}
