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
#define START_rx 17
#define FLAG_RCV_rx 18
#define A_RCV_rx 19

//definir estados controlo
#define C_RCV_CONTROL_rx 20
#define BCC_OK_CONTROL_rx 21
#define END_CONTROL_rx 22

//definir variáveis globais
#define FLAG 0x5c
#define A_SENDER 0x01
#define A_RECEIVER 0x03


//Variáveis de controlo
#define C_CONTROL_RECEIVER 0x06
#define C_CONTROL_SENDER 0x07
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
#define FLAG_RCV_INFO_rx 23
#define A_RCV_INFO_rx 24
#define S_C_RCV_INFO_rx 25
#define S_BCC_OK_1_INFO_rx 26


#define END_INFO_rx 27
#define REJ_INFO_rx 28

//Variáveis de informação
#define C_DISC 0x10
#define BCC_DISC_tx A_RECEIVER^C_DISC
#define BCC_DISC_rx A_SENDER^C_DISC

//definir estados de terminação
#define FLAG_RCV_DISC_rx 29
#define A_RCV_DISC_rx 30
#define C_RCV_DISC_rx 31
#define BCC_OK_DISC_rx 32
#define END_DISC_rx 33
#define FLAG_RCV_UA_rx 34
#define A_RCV_UA_rx 35
#define C_RCV_UA_rx 36
#define BCC_OK_UA_rx 37
#define END_UA_rx 38


volatile int STOP=FALSE;

int fd,c,res_control, res_info_rr, res_disc, res_disc_r, res_discua, res_discua_r, state;
int prev_res_info = 0, res_info = 0;
struct termios oldtio,newtio;
unsigned char buf_controlo[5];
unsigned char buf_info_rx[300];
unsigned char buf_info_rr[5];
unsigned char stored_data[300];
unsigned char buf_disc[5];
unsigned char buf_disc_ua[5];

unsigned char BCC_INFO3[1];

int pos = 0;
int s = 0;
unsigned char bcc2_aux;

int alarm_flag = 0;

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

    /*VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)*/

    tcflush(fd, TCIOFLUSH);

    sleep(1);
    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    //printf("New termios structure set\n");

    establish_connection_rx();

    receive_data_rx();

    send_rr_rx();

    termination_rx();
    
    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;

} 

void establish_connection_rx() {

    state = START_rx;

    printf("\nA tentar estabelecer conexão\n");

    while (state != END_CONTROL_rx)
    {
        res_control = read(fd,buf_controlo,1);   /* returns after 5 chars have been input */
        buf_controlo[res_control]=0; 
        switch (state){
            case START_rx:
                if( buf_controlo[0] == FLAG){
                    state = FLAG_RCV_rx;
                    printf("\nRecebi 1ª flag no controlo\n");
                }
                else{
                    state = START_rx;
                }
                break;
            case FLAG_RCV_rx:
                if (buf_controlo[0] == FLAG){
                    state = FLAG_RCV_rx;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if( buf_controlo[0] == A_SENDER){
                    state = A_RCV_rx;
                    printf("Recebi acknowledge no controlo\n");
                }                
                else{
                    state = START_rx;
                }
                break;
            case A_RCV_rx:
                if (buf_controlo[0] == FLAG){
                    state = FLAG_RCV_rx;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if (buf_controlo[0] == C_CONTROL_SENDER)
                {
                    state = C_RCV_CONTROL_rx;
                    printf("Recebi controlo no controlo\n");
                }
                else{
                    state = START_rx;
                }
                break;
            case C_RCV_CONTROL_rx:
                if (buf_controlo[0] == FLAG){
                    state = FLAG_RCV_rx;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if (buf_controlo[0] == BCC_CONTROL_rx)
                {
                    state = BCC_OK_CONTROL_rx;
                    printf("Recebi bcc no controlo\n");
                }
                else{
                    state = START_rx;
                }
                break;
            case BCC_OK_CONTROL_rx:
                if (buf_controlo[0] == FLAG)
                {
                    state = END_CONTROL_rx;
                    printf("Recebi 2ª flag no controlo\n");
                    printf("\nTrama de controlo recebida com sucesso\n");
         
                }
                else{
                    state = START_rx;              
                }
                break;
            default:
                break;
        }
    }

    if (state == END_CONTROL_rx)
    {
        printf("\nA enviar feedback de controlo");

        buf_controlo[0] = FLAG;
        buf_controlo[1] = A_RECEIVER;
        buf_controlo[2] = C_CONTROL_RECEIVER;
        buf_controlo[3] = buf_controlo[1]^buf_controlo[2];
        buf_controlo[4] = FLAG;

        res_control = write(fd,buf_controlo,5);
        printf("\n%d Control bytes written\n", res_control);

        /*for (int x = 0; x < 5; x++){
            printf("buf_controlo[%d]: %x\n", x, buf_controlo[x]);
        }*/

    }
}

void receive_data_rx() {
    if (state == END_CONTROL_rx) {

        printf("\nA receber trama de informação\n");

        while (state != END_INFO_rx) {
            prev_res_info = res_info;
            res_info = read(fd, buf_info_rx, 1); /* returns after 1 char has been input */
            buf_info_rx[res_info] = 0;
            //printf("buf_info_rx[0]: %x\n", buf_info_rx[0]);
            switch (state) {
                case END_CONTROL_rx:
                    if (buf_info_rx[0] == FLAG) {
                        state = FLAG_RCV_INFO_rx;
                        printf("\nRecebi 1ª flag info\n");
                    } else {
                        state = END_CONTROL_rx;
                    }
                    break;
                case FLAG_RCV_INFO_rx:
                    if (buf_info_rx[0] == A_SENDER) {
                        state = A_RCV_INFO_rx;
                        printf("Recebi acknowledge info\n");
                    } else {
                        state = END_CONTROL_rx;
                    }
                    break;
                case A_RCV_INFO_rx:
                    if (buf_info_rx[0] == S0) {
                        s = 0;
                        state = S_C_RCV_INFO_rx;
                        printf("Recebi controlo info\n");
                    } else if (buf_info_rx[0] == S1) {
                        s = 1;
                        state = S_C_RCV_INFO_rx;
                        printf("Recebi controlo info\n");
                    } else {
                        state = END_CONTROL_rx;
                    }
                    break;
                case S_C_RCV_INFO_rx:
                    if ((buf_info_rx[0] == (A_SENDER ^ S0)) || (buf_info_rx[0] == (A_SENDER ^ S1))) {
                        state = S_BCC_OK_1_INFO_rx;
                        printf("Recebi BCC1 info\n");
                    } else {
                        state = END_CONTROL_rx;
                    }
                    break;
                case S_BCC_OK_1_INFO_rx:
                    if (buf_info_rx[0] != FLAG) {
                        // unstuffing
                        if (buf_info_rx[0] == 0x5d) {
                            res_info = read(fd, buf_info_rx + 1, 1); // lê o byte seguinte
                            if (res_info == 1) {
                                if (buf_info_rx[1] == 0x7c) {
                                    stored_data[pos++] = FLAG; // substitui por 0x5c
                                } else if (buf_info_rx[1] == 0x7d) {
                                    stored_data[pos++] = 0x5d; // substitui por 0x5d
                                } else {
                                    // em caso de erro a dar unstuffing
                                    stored_data[pos++] = buf_info_rx[0];
                                    stored_data[pos++] = buf_info_rx[1];
                                }
                            } else {
                                // se falhar a leitura assume que é erro
                                stored_data[pos++] = buf_info_rx[0];
                            }
                        } else {
                            stored_data[pos++] = buf_info_rx[0];
                        }
                    } else {
                        BCC_INFO3[0] = stored_data[pos - 1];
                        for (int j = 0; j < pos - 1; j++) {
                            bcc2_aux ^= stored_data[j];
                            //printf("\nBCC2: %x\n", bcc2_aux);
                        }
                        //printf("BCC_INFO3[0] %x\n", BCC_INFO3[0]);
                        if (bcc2_aux == BCC_INFO3[0]) {
                            printf("Recebi BCC2 info\n");
                            printf("\nRecebi toda a trama info\n");
                            state = END_INFO_rx;
                        } else {
                            state = REJ_INFO_rx;
                            if (state == REJ_INFO_rx) {
                                bcc2_aux = 0;
                                printf("\nA enviar feedback REJ");

                                buf_info_rr[0] = FLAG;
                                buf_info_rr[1] = A_RECEIVER;
                                if (s == 0) {
                                    buf_info_rr[2] = REJ0;
                                } else if (s == 1) {
                                    buf_info_rr[2] = REJ1;
                                }
                                buf_info_rr[3] = buf_info_rr[1] ^ buf_info_rr[2];
                                buf_info_rr[4] = FLAG;

                                res_info_rr = write(fd, buf_info_rr, 5);
                                printf("\n%d Info bytes written\n", res_info_rr);
                                printf("Trama REJ enviada com sucesso\n");
                                state = END_CONTROL_rx;
                                alarm_flag++;
                                //pos = 0;
                                
                                receive_data_rx();
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void send_rr_rx(){

    if (state == END_INFO_rx)
    {
        printf("\nA enviar feedback RR");

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
        printf("\n%d Info bytes written\n", res_info_rr);
        printf("Trama RR enviada com sucesso\n");
    }
}

void termination_rx(){

    printf("\nA receber trama de terminação\n");

    if (state == END_INFO_rx)
    {
       while (state != END_DISC_rx)
        {
            res_disc = read(fd,buf_disc,1);   /* returns after 5 chars have been input */
            buf_disc[res_disc]=0; 
            switch (state){
                case END_INFO_rx:
                    if (buf_disc[0] == FLAG){
                        state = FLAG_RCV_DISC_rx;
                        printf("\nRecebi 1ª flag disc\n");
                    }
                    else{
                        state = END_INFO_rx;
                    }
                    break;
                case FLAG_RCV_DISC_rx:
                    if (buf_disc[0] == A_SENDER){
                        state = A_RCV_DISC_rx;
                        printf("Recebi acknowledge disc\n");
                    }
                    else{
                        state = END_INFO_rx;
                    }
                    break;
                case A_RCV_DISC_rx:
                    if (buf_disc[0] == C_DISC){
                        state = C_RCV_DISC_rx;
                        printf("Recebi controlo disc\n");
                    }
                    else{
                        state = END_INFO_rx;
                    }
                    break;
                case C_RCV_DISC_rx:
                    if (buf_disc[0] == BCC_DISC_rx){
                        state = BCC_OK_DISC_rx;
                        printf("Recebi BCC1 disc\n");
                    }
                    else{
                        state = END_INFO_rx;
                    }
                    break;
                case BCC_OK_DISC_rx:
                    if (buf_disc[0] == FLAG){
                        state = END_DISC_rx;
                        printf("Recebi 2ª flag disc\n");
                        printf("\nTrama de terminação recebida com sucesso\n");
                    }
                    else{
                        state = END_INFO_rx;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (state == END_DISC_rx){

        printf("\nA enviar feedback DISC");

        buf_disc[0] = FLAG;
        buf_disc[1] = A_RECEIVER;
        buf_disc[2] = C_DISC;
        buf_disc[3] = buf_disc[1]^buf_disc[2];
        buf_disc[4] = FLAG;

        res_disc = write(fd,buf_disc,5);
        printf("\n%d Disc bytes written\n", res_disc);
    }

    if (state == END_DISC_rx)
    {
        printf("\nA terminar ligação\n");

        while (state != END_UA_rx)
        {
            res_discua_r = read(fd,buf_disc_ua,1);   
            buf_disc_ua[res_discua_r]=0; 
            switch (state){
                case END_DISC_rx:
                    if (buf_disc_ua[0] == FLAG){
                        state = FLAG_RCV_UA_rx;
                        printf("\nRecebi 1ª flag UA\n");
                    }
                    else{
                        state = END_DISC_rx;
                    }
                    break;
                case FLAG_RCV_UA_rx:
                    if (buf_disc_ua[0] == A_SENDER){
                        state = A_RCV_UA_rx;
                        printf("Recebi acknowledge UA\n");
                    }
                    else{
                        state = END_DISC_rx;
                    }
                    break;
                case A_RCV_UA_rx:
                    if (buf_disc_ua[0] == C_CONTROL_RECEIVER){
                        state = C_RCV_UA_rx;
                        printf("Recebi controlo UA\n");
                    }
                    else{
                        state = END_DISC_rx;
                    }
                    break;
                case C_RCV_UA_rx:
                    if (buf_disc_ua[0] == BCC_CONTROL_rx){
                        state = BCC_OK_UA_rx;
                        printf("Recebi BCC2 UA\n");
                    }
                    else{
                        state = END_DISC_rx;
                    }
                    break;
                case BCC_OK_UA_rx:
                    if (buf_disc_ua[0] == FLAG){
                        state = END_UA_rx;
                        printf("Recebi 2ª flag UA\n");

                        printf("\nLIGAÇÃO TERMINADA\n");
                    }
                    else{
                        state = END_DISC_rx;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}