/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
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
#define BCC_CONTROL A_SENDER^C_CONTROL_RECEIVER

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

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
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
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

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
                if (buf_controlo[0] == FLAG){
                    state = FLAG_RCV;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if( buf_controlo[0] == A_SENDER){
                    state = A_RCV;
                    printf("Recebi acknowledge no controlo\n");
                }                
                else{
                    state = START;
                }
                break;
            case A_RCV:
                if (buf_controlo[0] == FLAG){
                    state = FLAG_RCV;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if (buf_controlo[0] == C_CONTROL_SENDER)
                {
                    state = C_RCV_CONTROL;
                    printf("Recebi controlo no controlo\n");
                }
                else{
                    state = START;
                }
                break;
            case C_RCV_CONTROL:
                if (buf_controlo[0] == FLAG){
                    state = FLAG_RCV;
                    printf("Recebi flag quando não devia no controlo\n");
                }
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
                    printf("\nTrama de controlo recebida com sucesso\n");
                }
                else{
                    state = START;              
                }
                break;
    
            default:
                break;
        }
    }

    

    if (state == END_CONTROL)
    {
        buf_controlo[0] = FLAG;
        buf_controlo[1] = A_RECEIVER;
        buf_controlo[2] = C_CONTROL_RECEIVER;
        buf_controlo[3] = buf_controlo[1]^buf_controlo[2];
        buf_controlo[4] = FLAG;


        res_control = write(fd,buf_controlo,5);
        printf("\n%d bytes written\n", res_control);
    }
    

    int pos = 0;
    int s = 0;
    unsigned char bcc2_aux = 0x00;


    if (state == END_CONTROL)
    {
        while (state != END_INFO)
        {
            res_info = read(fd,buf_info,1);   /* returns after 5 chars have been input */
            buf_info[res_info]=0; 
            switch (state){
                case END_CONTROL:
                    if (buf_info[0] == FLAG){
                        state = FLAG_RCV_INFO;
                        printf("\nRecebi 1ª flag info\n");

                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case FLAG_RCV_INFO:
                    if (buf_info[0] == A_SENDER){
                        state = A_RCV_INFO;
                        printf("Recebi acknowledge info\n");
                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case A_RCV_INFO:
                    if (buf_info[0] == S0){
                        s = 0;
                        state = S_C_RCV_INFO;
                        printf("Recebi controlo info\n");
                    }
                    else if (buf_info[0] == S1){
                        s = 1;
                        state = S_C_RCV_INFO;
                        printf("Recebi controlo info\n");
                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case S_C_RCV_INFO:
                    if ((buf_info[0]== BCC_INFO1) || (buf_info[0] == BCC_INFO2)){

                        state = S_BCC_OK_1_INFO;
                        printf("Recebi BCC1 info\n");

                    }
                    else{
                        state = END_CONTROL;
                    }
                    break;
                case S_BCC_OK_1_INFO:
                    if (buf_info[0]!= FLAG){
                        stored_data [pos] = buf_info[0];
                        //printf("stored data[%d]: %c\n", pos, stored_data [pos]);
                        pos++;
                    }
                    else if (buf_info[0] == FLAG){
                        
                        printf("Recebi dados info\n");
                        BCC_INFO3[0] = stored_data[pos-1];
                        
                        for (int j = 0; j < pos-1; j++)
                        {
                            bcc2_aux ^= stored_data[j];
                            //printf("bcc2: %c\n", bcc2_aux);
                        }

                        //printf("bcc2 dif: %c\n", bcc2_aux);

                        if (bcc2_aux == BCC_INFO3[0])
                        {
                            printf("Recebi BCC2 info\n");
                            printf("\nAcabei trama info\n");
                            state = END_INFO;
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }



    if (state == END_INFO)
    {
        buf_info_rr[0] = FLAG;
        buf_info_rr[1] = A_RECEIVER;
        if (s == 0)
        {
            buf_info_rr[2] = RR1;

        }
        else if (s == 1)
        {
            buf_info_rr[2] = RR0;

        }

        buf_info_rr[3] = buf_info_rr[1]^buf_info_rr[2];
        //printf("buf_info_rr[3]: %x\n", buf_info_rr[3]);
        buf_info_rr[4] = FLAG;


        res_info_rr = write(fd,buf_info_rr,5);
        /*for (int j = 0; j < 5; j++)
        {
            printf("\nbuf_info_rr[%d]: %x\n", j, buf_info_rr[j]);
        }*/
        
        printf("\n%d bytes written\n", res_info_rr);
        printf("\nTrama RR enviada com sucesso\n");

    }
    
    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
} 