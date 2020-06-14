
/******************************************************************
* pseudo-telnetd util ;)
* compile: 
*
* linux: gcc -Wall -O2 logind.c -o logind -lutil
* osx: gcc -Wall -O2 logind.c -o logind
* solaris: gcc -Wall -O2 logind.c -o logind -lnsl -lsocket
*
*******************************************************************/

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <utmp.h>
#include <sys/select.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(__gnu_linux__) 
#include <pty.h>
#endif 

#if defined(__APPLE__)
#include <util.h>
#endif



/*********************************
* macros aux, config
**********************************/

#define SHELL "/bin/sh"
#define PORT 12345
#define IP "127.0.0.1"
#define WORK_DIR "/tmp"
#define SZ_SBUF 512
#define LOG_FILE "/tmp/logind.log"

#define TRY(x, msg) if(x == -1) x_abort(msg " err")
#define LOG(x) write(f_log, x, strlen(x))


/*********************************
* variables globales
**********************************/

int pid, term_m, term_s, sock_c, sock_s, f_log;


/*********************************
* funciones 
**********************************/

// quitamos retornos 
void chomp(char *s)
{
	while (*s) if (*s == '\n' || *s == 0x0d) *s = 0;
		else s++; 
}

// salida limpia 
void clean()
{
	close(term_m);
	close(term_s);
	close(sock_c);
	close(sock_s);
	close(f_log);
}


// abort con msg
void x_abort(const char *msg)
{
	perror(msg);
	clean();
	exit(EXIT_FAILURE);
}


// manejador de interrupciones
void sig(int intr)
{
	printf("exit code: %d\n", intr);

	// segun int
	switch(intr) 
	{
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
		case SIGCHLD:

			// salida
			printf("logout!\n");

		break;
	}

	// salida limpia
	kill(pid, SIGTERM);
	clean();
	exit(0);
}


// funcion principal
int main()
{
	struct winsize win;
	struct termios tt;
	char *const args[] = { SHELL, "-i", 0 };
	fd_set  rfd;
	int r, sz, on = 1;
	static char sbuf[SZ_SBUF];
	struct sockaddr_in ads, adc;
	socklen_t len;
#if defined(__SVR4)
	int pgrp; 
	char *ts_name;
#endif 

	// parametros terminal
	tcgetattr(0, &tt);
	ioctl(0, TIOCGWINSZ, &win);

	// abrimos fich log
	TRY((f_log = open(LOG_FILE, O_WRONLY | 
		O_CREAT | O_APPEND, 0644)), "open");

	LOG("logind started!\n");

	//
	// primer fork (daemon)
	// 

	LOG("fork 1!\n");
	TRY((pid = fork()), "fork"); 

	// padre? salimos
	if (pid) return 0;

	//
	// a partir de aqui, hijo
	//

	// creamos una nueva sesion
	LOG("setsid!\n");
	TRY(setsid(), "setsid");

	// directorio de trabajo
	LOG("chdir!\n");
	TRY(chdir(WORK_DIR), "chdir");

#if defined(__gnu_linux__) || defined(__APPLE__)
	// creamos pseudo-terminales
	LOG("openpty!\n");
	TRY(openpty(&term_m, &term_s, NULL, &tt, &win), "opentty");
#endif 

#if defined(__SVR4)
	LOG("open term!\n");
	TRY((term_m = open("/dev/ptmx", O_RDWR)), "open");
	TRY(unlockpt(term_m), "unlockpt");
	ts_name = (char *)ptsname(term_m);
	TRY(grantpt(term_m), "grantpt");
#endif 

	// instalamos manejador ints 
	LOG("set signals!\n");
	signal(SIGTERM, sig);
	signal(SIGINT, sig);
	signal(SIGQUIT, sig);
	signal(SIGCHLD, sig);

	// tercer fork 
	LOG("fork 2!\n");
	TRY((pid = fork()), "fork");

	// hijo?
	if (!pid)
	{
		// cerramos term_m
		close(term_m);

#if defined(__gnu_linux__) || defined(__APPLE__)
		// preparamos tty
		LOG("login_tty!\n");
		TRY(login_tty(term_s), "login_tty");
#endif 

#if defined(__SVR4) 
		// creamos una nueva sesion
		LOG("setsid!\n");
		TRY((pgrp = setsid()), "setsid");

		// abrimos slave
		LOG("open term!\n");
		TRY((term_s = open(ts_name, O_RDWR)), "open");

		// duplica descriptores
		LOG("dup2!\n");
		dup2(term_s, 0);
		dup2(term_s, 1);
		dup2(term_s, 2);

		// set foreground the main process
		LOG("tcsetpgrp!\n");
		TRY(tcsetpgrp(0, pgrp), "tcsetpgrp");

		// flags necesarios ???
		tt.c_lflag = ISIG | ICANON | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
		tt.c_oflag = TABDLY | OPOST;
		tt.c_iflag = BRKINT | IGNPAR | ISTRIP | ICRNL | IXON | IMAXBEL;
		tt.c_cflag = CBAUD | CS8 | CREAD;
#endif

		// desactivamos echo
		LOG("tcsetattr!\n");
		tt.c_lflag &= ~ECHO;
		tcsetattr(0, TCSANOW, &tt);
	
/*
		// auth cutre ;)
		do
		{
			// prompt
			strcpy(sbuf, "password: "); 
			sz = strlen(sbuf);
			write(1, sbuf, sz);

			// leido contrasenia?
			if ((sz = read(0, sbuf, SZ_SBUF)))
			{
				// debug
				// int i; for (i = 0; i < sz; i++) printf("%02x ", sbuf[i]); printf("\n");

				// pendiente: porque empieza por 0 ???
				char *idx = (sz && sbuf[0] == 0) ? sbuf + 1 : sbuf;

				// quitamos retornos
				chomp(idx);

				// es valida? 
				if (strncmp(idx, "12345", SZ_SBUF))
				{
					strcpy(sbuf, "invalid password!\n");
					sz = strlen(sbuf);
					write(1, sbuf, sz);
				}

				else 
				{
					strcpy(sbuf, "connected!\n");
					sz = strlen(sbuf);
					write(1, sbuf, sz);

					break;
				}

			}

		} while (1); 
		
*/
/*
		255 251 34 -> Interpret As Command, Will, Linemode

		0xfb IAC WILL
		0xfc IAC WONT
		0xfd IAC DO
		0xfe IAC DONT

*/
		// IAC WILL suppress go-ahead (33)
		//write(1, "\xff\xfb\x03", 3);

		// activamos echo 
		tt.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &tt);

		// lanzamos shell
		LOG("execv!\n");
		TRY(execv(SHELL, args), "execv");

		// llegados aqui, algo mal?
		LOG("execv error!\n");
		clean();
		kill(0, SIGTERM);
		exit(0);
	}

	// 
	// a partir de aqui, padre 
	//

	// abrimos socket 
	LOG("socket!\n");
	TRY((sock_s = socket(AF_INET, SOCK_STREAM, 0)), "socket");

	// parametros conexion
	memset(&ads, 0, sizeof(ads));
	ads.sin_family = AF_INET;
	ads.sin_port = htons(PORT);
	ads.sin_addr.s_addr = inet_addr(IP);

	// permitimos reutilizacion ip:port (time_wait)
	LOG("setsockopt!\n");
	TRY(setsockopt(sock_s, SOL_SOCKET, SO_REUSEADDR,
		(const char *) &on, sizeof(on)), "setsockopt");

	// bind a ip:puerto
	LOG("bind!\n");
	TRY((bind(sock_s, (struct sockaddr *)&ads, sizeof(ads))), "bind");

	// modo escucha
	LOG("listen!\n");
	TRY(listen(sock_s, 1), "listen");

	// aceptamos conexiones
	LOG("accept!\n");
	len = sizeof(adc);
	TRY((sock_c = accept(sock_s, (struct sockaddr *)&adc, &len)), "acept");

	// inicializamos rfd
	FD_ZERO(&rfd);

#if defined(__gnu_linux__) || defined(__APPLE__)
	// IAC WILL ECHO
	write(sock_c, "\xff\xfb\x01", 3);
	read(sock_c, sbuf, 3);

	// IAC WILL suppress go-ahead (33)
	write(sock_c, "\xff\xfb\x03", 3);
	read(sock_c, sbuf, 3);
#endif

	// hasta signal
	while (1)
	{
		LOG("read socket <-> term!\n");

		FD_SET(term_m, &rfd);
		FD_SET(sock_c, &rfd);

		// i/o check
		r = select((term_m > sock_c ? term_m : sock_c) + 1, 
			&rfd, 0, 0, NULL);

		// preparado ?
		if (r > 0 || errno == EINTR) 
		{
			// lectura socket
			if (FD_ISSET(sock_c, &rfd))
			{
				// sock -> term
				if ((sz = read(sock_c, sbuf, SZ_SBUF)) > 0) 
					write(term_m, sbuf, sz);

				// salimos ?
				else break;
			}

			// lectura tty
			if (FD_ISSET(term_m, &rfd))
			{
				// term -> sock
				if ((sz = read(term_m, sbuf, SZ_SBUF)) > 0) 
					write(sock_c, sbuf, sz);
			}
		}
	}

	// nunca deberia llegar 
	return 0;
}
