#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>


void handler(int sig);
void cerrarHijos(tline* line, int** hijos);
void redirecciones(tline* line);
void cd(tline* line);

int pid;
int main(int argc, char ** argv) {
	char buf[1024];
	char direccion[1024];
	tline * line;
	printf("%s msh> ", getcwd(direccion, 1024));
	while (fgets(buf, 1024, stdin)){
		char directorio[1024];
		int **hijos;
		line = tokenize(buf);
		if (line==NULL) {
			continue;
		}
		//Ejecutar CD
		if(strcmp(line->commands[0].argv[0], "cd") == 0){
			cd(line);
			printf("%s msh> ", getcwd(direccion, 1024));
			continue;
		}
		//Exit
		if(strcmp(line->commands[0].argv[0], "exit") == 0)
			break;
		signal(SIGQUIT, handler);
		signal(SIGINT, handler);
		//Reservamos memoria para los hijos
		hijos = (int**)malloc((line->ncommands-1)*sizeof(int*));
		for(int i=0;i<line->ncommands-1;i++){
			hijos[i]=(int*)malloc(2*sizeof(int));
			pipe(hijos[i]);	
		}
		
		int restaurarStdin;
		int restaurarStdout;
		int restaurarStderr;
		//Duplicamos los descriptores estándar para despues restaurarlos
		dup2(STDIN_FILENO, restaurarStdin);
		dup2(STDOUT_FILENO, restaurarStdout);
		dup2(STDERR_FILENO, restaurarStderr);
		//Captura redireccion (INPUT)
		redirecciones(line);		
		for(int i=0; i<line->ncommands; i++){
			pid=fork();
			if(pid<0){
				fprintf(stderr, "ERROR en el fork()\n");			
			}
			else if(pid==0){
				//Para un solo comando			
				if(line->ncommands==1){
					redirecciones(line);
				} 
				else if(line->ncommands>1){
					if(i==0){
						dup2(hijos[i][1], STDOUT_FILENO);		
					}
					else if(i!=(line->ncommands-1)){				
						dup2(hijos[i][1], STDOUT_FILENO);
						dup2(hijos[i-1][0], STDIN_FILENO);			
					}
					else{
						
						dup2(hijos[i-1][0], STDIN_FILENO);
						//Captura redirecciones (OUTPUT)
						redirecciones(line);
					}			
				}
			
				//Cerramos las pipes ya que sino se queda esperando
				cerrarHijos(line, hijos);
				execvp(line->commands[i].filename, line->commands[i].argv);
			}
		}
		//Cerramos las pipes
		cerrarHijos(line, hijos);
		//Wait para que se escriba bien el prompt
		for(int i=0;i<line->ncommands;i++){
			wait(hijos[i]);			
			
		}
		//Restauramos los desciptores estándar para la siguiente ejecución
		dup2(restaurarStdin, STDIN_FILENO);
		dup2(restaurarStdout, STDOUT_FILENO);
		dup2(restaurarStderr, STDERR_FILENO);
		printf("%s msh> ", getcwd(direccion, 1024));
		free(hijos);	
	}
	
	return 0;
}

void cd(tline* line){
		
			if(line->commands[0].argv[1]!=NULL){
				chdir(line->commands[0].argv[1]);
			}
			else{
				chdir(getenv("HOME"));
			}	
}
void redirecciones(tline* line){
	int fd_in;
	int fd_out;
	int fd_err;
	if(line->redirect_input != NULL){
			fd_in=open(line->redirect_input, O_RDONLY);
			if(fd_in==-1){
				fprintf(stderr, "%s: ERROR al abrir el archivo\n", line->redirect_input);
			}
			else {
			dup2(fd_in,STDIN_FILENO);
			}
		}
		if(line->redirect_error!=NULL){ 
			fd_err=open(line->redirect_error, O_CREAT | O_WRONLY | O_TRUNC, 0664);
			dup2(fd_err, STDERR_FILENO);		
		
		}
		if(line->redirect_output!=NULL){				
			fd_out=open(line->redirect_output, O_CREAT | O_WRONLY | O_TRUNC, 0664);
			dup2(fd_out, STDOUT_FILENO);
		}


}
void cerrarHijos(tline* line, int** hijos){
	for(int i=0; i<(line->ncommands-1);i++){
		for(int j=0; j<2; j++){
			close(hijos[i][j]);				
		}
	}

}
void handler(int sig){
	if(pid==0){
		signal(sig, SIG_DFL);		
	}
	else{
		signal(sig, SIG_IGN);	
	}

}
