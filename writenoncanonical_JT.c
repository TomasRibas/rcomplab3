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
#define START_tx 0
#define FLAG_RCV_tx 1
#define A_RCV_tx 2

//definir estados controlo
#define C_RCV_CONTROL_tx 3
#define BCC_OK_CONTROL_tx 4
#define END_CONTROL_tx 5

//definir variáveis globais
#define FLAG 0x5c
#define A_SENDER 0x01
#define A_RECEIVER 0x03

//Variáveis de controlo
#define C_CONTROL_RECEIVER 0x06
#define C_CONTROL_SENDER 0x07
#define BCC_CONTROL_tx A_RECEIVER^C_CONTROL_RECEIVER
#define BCC_CONTROL_rx A_SENDER^C_CONTROL_SENDER

//Variáveis de informação
#define S0 0x80
#define S1 0xC0
#define BCC_INFO1 A_SENDER^S0
#define BCC_INFO2 A_SENDER^S1
#define RR0 0x01
#define RR1 0x11
#define BCC_INFO_RR0 A_RECEIVER^RR0
#define BCC_INFO_RR1 A_RECEIVER^RR1
#define REJ0 0x05
#define REJ1 0x15

//definir estados informação
#define FLAG_RCV_INFO_tx 6
#define A_RCV_INFO_tx 7
#define S_C_RCV_INFO_tx 8
#define S_BCC_OK_1_INFO_tx 9
#define END_INFO_tx 10
#define SEND_INFO_tx 11

//Variáveis de informação
#define C_DISC 0x10
#define BCC_DISC_tx A_RECEIVER^C_DISC
#define BCC_DISC_rx A_SENDER^C_DISC


//definir estados de terminação
#define FLAG_RCV_DISC_tx 12
#define A_RCV_DISC_tx 13
#define C_RCV_DISC_tx 14
#define BCC_OK_DISC_tx 15
#define END_DISC_tx 16


volatile int STOP=FALSE;

int fd,c,res_control,res_info, res_info_rr, res_disc, res_disc_r, res_discua, res_discua_r, state; 
struct termios oldtio,newtio;
unsigned char buf_controlo[5];
unsigned char buf_info[300];
unsigned char buf_info_rr[5];
unsigned char stored_data[300];
unsigned char buf_disc[5];
unsigned char buf_disc_ua[5];

unsigned char BCC_INFO3[1];

int i, sum = 0, speed = 0;

int aux = 2;
unsigned char bcc2_aux;

//TO DO LIST
//->rej e resend - DONE
//->byte stuffing e unstuffing - DONE
//->interligar na aplication
//->fazer os timeouts

int main(int argc, char** argv)
{
    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS10", argv[1])!=0) &&
          (strcmp("/dev/ttyS11", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    /*Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.*/

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
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */


    /*VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)*/

    tcflush(fd, TCIOFLUSH);
    
    sleep(1);
    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    //printf("New termios structure set\n");

    establish_connection_tx();

    send_data_tx();

    feedback_data_tx();

    termination_tx();
    
    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;

}

void establish_connection_tx(){


    buf_controlo[0] = FLAG;
    buf_controlo[1] = A_SENDER;//era 0x03
    buf_controlo[2] = C_CONTROL_SENDER;//era 0x08
    buf_controlo[3] = buf_controlo[1]^buf_controlo[2];
    buf_controlo[4] = FLAG;
    
    printf("\nA tentar estabelecer conexão");

    res_control = write(fd,buf_controlo,5);
    printf("\n%d Control bytes written\n", res_control);

    /*for (int x = 0; x < 5; x++)
    {
        printf("buf_controlo[%d]: %x\n", x, buf_controlo[x]);
    }*/

    state = START_tx;
    while (state != END_CONTROL_tx)
    {
        res_control = read(fd,buf_controlo,1);   /* returns after 5 chars have been input */
        buf_controlo[res_control]=0; 
        switch (state){
            case START_tx:
                if( buf_controlo[0] == FLAG){
                    state = FLAG_RCV_tx;
                    printf("\nRecebi 1ª flag no controlo\n");
                }
                else{
                    state = START_tx;
                }
                break;
            case FLAG_RCV_tx:
                if( buf_controlo[0] == A_RECEIVER){
                    state = A_RCV_tx;
                    printf("Recebi acknowledge no controlo\n");
                }
                else{
                    state = START_tx;
                }
                break;
            case A_RCV_tx:
                if (buf_controlo[0] == C_CONTROL_RECEIVER)
                {
                    state = C_RCV_CONTROL_tx;
                    printf("Recebi controlo no controlo\n");
                }
                else{
                    state = START_tx;
                }
                break;
            case C_RCV_CONTROL_tx:
                if (buf_controlo[0] == BCC_CONTROL_tx)
                {
                    state = BCC_OK_CONTROL_tx;
                    printf("Recebi BCC no controlo\n");
                }
                else{
                    state = START_tx;
                }
                break;
            case BCC_OK_CONTROL_tx:
                if (buf_controlo[0] == FLAG)
                {
                    state = END_CONTROL_tx;
                    printf("Recebi 2ª flag no controlo\n");
                    printf("\nLigação estabelecida com sucesso\n");                   
                }
                else{
                    state = START_tx;
                }
                break;
    
            default:
                break;
        }
    }
}

void send_data_tx(){

    printf("\nVou começar a enviar trama de informação");

    if (state == END_CONTROL_tx)
    {
        
        buf_info[0] = FLAG;
        buf_info[1] = A_SENDER;
        
        if(aux % 2 == 0){
            buf_info[2] = S0;
            aux++;
        }
        else{
            buf_info[2] = S1;
            aux++;
        }
        buf_info[3] = buf_info[1]^buf_info[2];

        for ( i = 4; i <253 ; i++)
        {
            buf_info[i] = ('A'+ i)%255 ;
            bcc2_aux ^= buf_info[i];
            //buf_info[i] = 'A' ;
            if (buf_info[i] == FLAG)
            {
                printf("\nTEVE FLAG NOS DADOS\n");
                buf_info[i] = 0x5d;
                buf_info[i+1] = 0x7c;
                i++;
            }
            if (buf_info[i] == 0x5d)
            {
                printf("\nESCD NOS DADOS\n");
                buf_info[i] = 0x5d;
                buf_info[i+1] = 0x7d;
                i++;
            }
            
            //printf("\nBCC2: %x\n", bcc2_aux);
        }
        if (bcc2_aux == FLAG)
        {
            buf_info[253] = 0x5d;
            buf_info[254] = 0x7c;
            buf_info[255] = FLAG;
        }
        else if (bcc2_aux == 0x5d)
        {
            buf_info[253] = 0x5d;
            buf_info[254] = 0x7d;
            buf_info[255] = FLAG;
        }else{
            buf_info[253] = bcc2_aux;
            buf_info[254] = FLAG;
        }
        
    
        //printf("\nBCC2: %x\n", bcc2_aux);
        
        res_info = write(fd,buf_info,255);
        printf("\n%d Info bytes written\n", res_info);

        /*for (int z = 0; z < 255; z++)
        {
            printf("buf_info[%d]: %x\n", z, buf_info[z]);
        }*/

        state = SEND_INFO_tx;
    }
}

void feedback_data_tx(){

    if (state == SEND_INFO_tx){  
        printf("\nA receber feedback RR\n");
        
        while (state != END_INFO_tx)
        {
            //printf("state= %d\n", state);
            res_info_rr = read(fd,buf_info_rr,1);
            buf_info_rr[res_info_rr]=0;
            switch (state){
                case SEND_INFO_tx:
                    if (buf_info_rr[0] == FLAG)
                    {
                        state = FLAG_RCV_INFO_tx;
                        printf("\nRecebi 1ª flag RR\n");
                    }
                    else{
                        state == SEND_INFO_tx;
                    }                
                    break;
                case FLAG_RCV_INFO_tx:
                    if (buf_info_rr[0] == A_RECEIVER)
                    {
                        state = A_RCV_INFO_tx;
                        printf("Recebi acknowledge RR\n");
                    }
                    else{
                        state == SEND_INFO_tx;
                    }
                    break;
                case A_RCV_INFO_tx:
                    if (buf_info_rr[0] == RR0)
                    {
                        state = S_C_RCV_INFO_tx;
                        printf("Recebi RR0\n");
                    }
                    else if (buf_info_rr[0] == RR1)
                    {
                        state = S_C_RCV_INFO_tx;
                        printf("Recebi RR1\n");
                    }
                    else if (buf_info_rr[0] == REJ0 || REJ1)
                    {
                        state = END_CONTROL_tx;
                        printf("Recebi REJ\n");
                        send_data_tx();
                    }
                    else{
                        state == SEND_INFO_tx;
                    }
                    break;
                case S_C_RCV_INFO_tx:
                    //printf("RR: %x\n", buf_info_rr[0]);
                    if (buf_info_rr[0] == (A_RECEIVER^RR0))
                    {
                        state = S_BCC_OK_1_INFO_tx;
                        printf("Recebi BCC RR0\n");
                    }
                    else if (buf_info_rr[0] == (A_RECEIVER^RR1))
                    {                        
                        state = S_BCC_OK_1_INFO_tx;
                        printf("Recebi BCC RR1\n");
                    }
                    else{
                        state == SEND_INFO_tx;
                    }
                    break;
                case S_BCC_OK_1_INFO_tx:
                    if (buf_info_rr[0] == FLAG)
                    {
                        state = END_INFO_tx;
                        printf("Recebi 2ª flag RR\n");
                        printf("\nInformação enviada com sucesso\n");
                    }
                    else{
                        state == SEND_INFO_tx;
                    }
                    break;
        
                default:
                    break;
            }   
        } 
    }
}

void termination_tx(){

    if (state == END_INFO_tx){
        printf("\nVou enviar trama de terminação");

        buf_disc[0] = FLAG;
        buf_disc[1] = A_SENDER;
        buf_disc[2] = C_DISC;
        buf_disc[3] = buf_disc[1]^buf_disc[2];
        buf_disc[4] = FLAG;

        res_disc = write(fd,buf_disc,5);
        printf("\n%d Disc bytes written\n", res_disc);
    }

    if (state == END_INFO_tx)
    {
       printf("\nA receber feedback de terminação\n");
       while (state != END_DISC_tx)
        {
            res_disc = read(fd,buf_disc,1);   /* returns after 5 chars have been input */
            buf_disc[res_disc]=0; 
            switch (state){
                case END_INFO_tx:
                    if (buf_disc[0] == FLAG){
                        state = FLAG_RCV_DISC_tx;
                        printf("\nRecebi 1ª flag disc\n");
                    }
                    else{
                        state = END_INFO_tx;
                    }
                    break;
                case FLAG_RCV_DISC_tx:
                    if (buf_disc[0] == A_RECEIVER){
                        state = A_RCV_DISC_tx;
                        printf("Recebi acknowledge disc\n");
                    }
                    else{
                        state = END_INFO_tx;
                    }
                    break;
                case A_RCV_DISC_tx:
                    if (buf_disc[0] == C_DISC){
                        state = C_RCV_DISC_tx;
                        printf("Recebi controlo disc\n");
                    }
                    else{
                        state = END_INFO_tx;
                    }
                    break;
                case C_RCV_DISC_tx:
                    if (buf_disc[0] == BCC_DISC_tx){
                        state = BCC_OK_DISC_tx;
                        printf("Recebi BCC1 disc\n");
                    }
                    else{
                        state = END_INFO_tx;
                    }
                    break;
                case BCC_OK_DISC_tx:
                    if (buf_disc[0] == FLAG){
                        state = END_DISC_tx;
                        printf("Recebi 2ª flag disc\n");
                        printf("\nTrama de terminação recebida com sucesso\n");
                    }
                    else{
                        state = END_INFO_tx;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (state == END_DISC_tx)
    {
        printf("\nA terminar ligação");
        buf_disc_ua[0] = FLAG;
        buf_disc_ua[1] = A_SENDER;
        buf_disc_ua[2] = C_CONTROL_RECEIVER;
        buf_disc_ua[3] = buf_disc_ua[1]^buf_disc_ua[2];
        buf_disc_ua[4] = FLAG;
        
        res_discua_r = write(fd,buf_disc_ua,5);
        printf("\n%d UA bytes written\n", res_discua_r);

        printf("\nLIGAÇÃO TERMINADA\n");
    }
}