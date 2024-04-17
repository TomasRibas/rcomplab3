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

//definir estados globais
#define START 0
#define FLAG_RCV 1
#define A_RCV 2

//definir estados controlo
#define C_RCV_CONTROL 3
#define BCC_OK_CONTROL 4
#define END_CONTROL 5

//definir variáveis globais
#define FLAG 0x5c
#define A_SENDER 0x01
#define A_RECEIVER 0x03

//Variáveis de controlo
#define C_CONTROL_RECEIVER 0x06
#define C_CONTROL_SENDER 0x07
#define BCC_CONTROL A_RECEIVER^C_CONTROL_RECEIVER

//Variáveis de informação
#define S0 0x80
#define S1 0xC0
#define BCC_INFO1 A_SENDER^S0
#define BCC_INFO2 A_SENDER^S1
#define RR0 0x01
#define RR1 0x11
#define BCC_INFO_RR0 A_RECEIVER^RR0
#define BCC_INFO_RR1 A_RECEIVER^RR1

//definir estados informação
#define FLAG_RCV_INFO 6
#define A_RCV_INFO 7
#define S_C_RCV_INFO 8
#define S_BCC_OK_1_INFO 9
#define S_DATA_INFO 10
#define S_BCC_OK_2_INFO 11
#define R_C_RCV_INFO 12
#define END_INFO 13

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd,c,res_control,res_info, res_info_rr, state; 
    struct termios oldtio,newtio;
    unsigned char buf_controlo[5];
    unsigned char buf_info[255];
    unsigned char buf_info_rr[5];
    unsigned char stored_data[255];

    unsigned char BCC_INFO3[1];

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

    buf_controlo[0] = FLAG;
    buf_controlo[1] = A_SENDER;//era 0x03
    buf_controlo[2] = C_CONTROL_SENDER;//era 0x08
    buf_controlo[3] = buf_controlo[1]^buf_controlo[2];
    buf_controlo[4] = FLAG;
    

    res_control = write(fd,buf_controlo,5);
    printf("\n%d bytes written\n", res_control);

    
    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }
    
    state = START;
    while (state != END_CONTROL)
    {
        res_control = read(fd,buf_controlo,1);   /* returns after 5 chars have been input */
        buf_controlo[res_control]=0; 
        switch (state){
            case START:
                if( buf_controlo[0] == FLAG){
                    state = FLAG_RCV;
                    printf("\nRecebi 1ª flag no controlo\n");
                }
                else{
                    state = START;
                }
                break;
            case FLAG_RCV:
                if( buf_controlo[0] == A_RECEIVER){
                    state = A_RCV;
                    printf("Recebi acknowledge no controlo\n");
                }
                else{
                    state = START;
                }
                break;
            case A_RCV:
                if (buf_controlo[0] == C_CONTROL_RECEIVER)
                {
                    state = C_RCV_CONTROL;
                    printf("Recebi controlo no controlo\n");
                }
                else{
                    state = START;
                }
                break;
            case C_RCV_CONTROL:
                if (buf_controlo[0] == BCC_CONTROL)
                {
                    state = BCC_OK_CONTROL;
                    printf("Recebi bcc no controlo\n");
                }
                else{
                    state = START;
                }
                break;
            case BCC_OK_CONTROL:
                if (buf_controlo[0] == FLAG)
                {
                    state = END_CONTROL;
                    printf("Recebi 2ª flag no controlo\n");
                    printf("\nLigação estabelecida com sucesso\n");
                    
                }
                else{
                    state = START;
                }
                break;
    
            default:
                break;
        }
    }

    int aux = 2;

    printf("\nVou começar a enviar trama de informação\n");
    
    if (state == END_CONTROL)
    {
        
        buf_info[0] = FLAG;
        buf_info[1] = A_SENDER;
        
        if(aux % 2 == 0){
            buf_info[2] = S0;
            aux++;
        }
        else{
            buf_info[2] = S1;
        }
        buf_info[3] = buf_info[1]^buf_info[2];

        for ( i = 4; i <255 ; i++)
        {
            buf_info[i] = 'O';
        }

        buf_info[253] = 'T';
        buf_info[254] = FLAG;

        res_info = write(fd,buf_info,255);
        printf("%d info bytes written\n", res_info);
    }


    if (state == END_CONTROL){
        while (state != END_INFO)
        {
            res_info_rr = read(fd,buf_info_rr,1);
            buf_info_rr[res_info_rr]=0;
            switch (state){
                case END_CONTROL:
                    if (buf_info_rr[0] == FLAG)
                    {
                        state = FLAG_RCV_INFO;
                        printf("Recebi 1ª flag RR\n");
                    }
                    else{
                        state = END_CONTROL;
                    }                
                    break;
                case FLAG_RCV_INFO:
                    if (buf_info_rr[0] == A_RECEIVER)
                    {
                        state = A_RCV_INFO;
                        printf("Recebi acknowledge RR\n");
                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case A_RCV_INFO:
                    if (buf_info_rr[0] == RR0)
                    {
                        state = S_C_RCV_INFO;
                        printf("Recebi RR0\n");
                    }
                    else if (buf_info_rr[0] == RR1)
                    {
                        state = S_C_RCV_INFO;
                        printf("Recebi RR1\n");
                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case S_C_RCV_INFO:
                    if (buf_info_rr[0] == BCC_INFO_RR0)
                    {
                        state = S_BCC_OK_1_INFO;
                        printf("Recebi bcc RR0\n");
                    }
                    else if (buf_info_rr[0] == BCC_INFO_RR1)
                    {
                        state = S_BCC_OK_1_INFO;
                        printf("Recebi bcc RR1\n");
                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case S_BCC_OK_1_INFO:
                    if (buf_info_rr[0] == FLAG)
                    {
                        state = END_INFO;
                        printf("Recebi 2ª flag RR\n");
                        printf("\nInformação enviada com sucesso\n");
                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
        
                default:
                    break;
            }
        } 
    }
    
    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
