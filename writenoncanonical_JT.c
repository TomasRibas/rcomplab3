/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

//definir estados
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define END 5

#define FLAG 0x5c
#define A 0x01
#define C 0x06
#define BCC A^C	

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd,c, res, state; 
    struct termios oldtio,newtio;
    unsigned char buf[5];
    int i, sum = 0, speed = 0;

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS10", argv[1])!=0) &&
          (strcmp("/dev/ttyS11", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }


    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */


    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */


    tcflush(fd, TCIOFLUSH);
    
    sleep(1);
    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    buf[0] = 0x5c;
    buf[1] = 0x03;//era 0x03
    buf[2] = 0x08;//era 0x08
    buf[3] = buf[1]^buf[2];
    buf[4] = 0x5c;
    //buf[5] = '\n';

    res = write(fd,buf,5);
    printf("\n%d bytes written\n", res);

    /*
    O ciclo FOR e as instruções seguintes devem ser alterados de modo a respeitar
    o indicado no guião
    */
    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }
    
    state = START;
    while (state != END)
    {
        res = read(fd,buf,1);   /* returns after 5 chars have been input */
        buf[res]=0; 
        switch (state){
            case START:
                if( buf[0] == FLAG){
                    state = FLAG_RCV;
                    printf("\nRecebi 1ª flag\n");
                }
                else{
                    state = START;
                }
                break;
            case FLAG_RCV:
                if( buf[0] == A){
                    state = A_RCV;
                    printf("Recebi acknowledge\n");
                }
                else{
                    state = START;
                }
                break;
            case A_RCV:
                if (buf[0] == C)
                {
                    state = C_RCV;
                    printf("Recebi controlo\n");
                }
                else{
                    state = START;
                }
                break;
            case C_RCV:
                if (buf[0] == BCC)
                {
                    state = BCC_OK;
                    printf("Recebi bcc\n");
                }
                else{
                    state = START;
                }
                break;
            case BCC_OK:
                if (buf[0] == FLAG)
                {
                    state = END;
                    printf("Recebi 2ª flag \n");
                    printf("\nTrama enviada com sucesso\n");
                    
                }
                else{
                    state = START;
                }
                break;
    
            default:
                break;
        }
    }
    
    close(fd);
    return 0;
}
