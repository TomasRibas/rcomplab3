#include "linklayer.h"

#define FRAME_SIZE 5
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

//definir estados globais
#define START_rx 17
#define FLAG_RCV_rx 18
#define A_RCV_rx 19

//definir estados controlo
#define C_RCV_CONTROL_rx 20
#define BCC_OK_CONTROL_rx 21
#define END_CONTROL_rx 22

//definir estados informação 
#define FLAG_RCV_INFO_rx 23
#define A_RCV_INFO_rx 24
#define S_C_RCV_INFO_rx 25
#define S_BCC_OK_1_INFO_rx 26

#define END_INFO_rx 27
#define REJ_INFO_rx 28

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

#define _POSIX_SOURCE 1

int fd,c,res_control,res_info, res_info_rr, res_disc, res_disc_r, res_discua, res_discua_r;
int prev_res_info = 0, res_info = 0;
int state_rx, state_tx; 

int alarmFlag = FALSE, alarmCounter = 0, timeout = 0, retrans_data_counter = 0, s = 0, numTries = 1;

struct termios oldtio,newtio;
linkLayer connection;


unsigned char buf_controlo[6];
unsigned char buf_info_rx[300];

unsigned char buf_info_rr[6];
unsigned char stored_data[300];
unsigned char buf_disc[6];
unsigned char buf_disc_ua[6];
unsigned char bcc2_aux;

int pos = 0;

int i, sum = 0, speed = 0;

//tudo do lado do emissor, tudo o que for enviar SET, I e DISC ->verifica se envia tudo dentro do tempo necessário
void alarmHandler(int signal) {
    printf("\nAlarm counter number:%d\n", alarmCounter);
    alarmFlag = FALSE;  
    alarmCounter++;
}

int resetAlarm() {
    alarmCounter = 0;
    alarmFlag = FALSE;
}

int establish_connection_tx(){

    buf_controlo[0] = FLAG;
    buf_controlo[1] = A_SENDER;//era 0x03
    buf_controlo[2] = C_CONTROL_SENDER;//era 0x08
    buf_controlo[3] = buf_controlo[1]^buf_controlo[2];
    buf_controlo[4] = FLAG;

    (void)signal(SIGALRM, alarmHandler);

    printf("\nA tentar estabelecer conexão");

    res_control = write(fd,buf_controlo,5);
    alarm(connection.timeOut); 
    alarmFlag = TRUE;
    printf("\n%d Control bytes written\n", res_control);

    state_tx = START_tx;
    while (state_tx != END_CONTROL_tx)
    {
        res_control = read(fd,buf_controlo,1);   /* returns after 5 chars have been input */
        buf_controlo[res_control]=0; 
        switch (state_tx){
            case START_tx:
                if( buf_controlo[0] == FLAG){
                    state_tx = FLAG_RCV_tx;
                    printf("\nRecebi 1ª flag no controlo\n");
                }
                else{
                    state_tx = START_tx;
                }
                break;
            case FLAG_RCV_tx:
                if( buf_controlo[0] == A_RECEIVER){
                    state_tx = A_RCV_tx;
                    printf("Recebi acknowledge no controlo\n");
                }
                else{
                    state_tx = START_tx;
                }
                break;
            case A_RCV_tx:
                if (buf_controlo[0] == C_CONTROL_RECEIVER)
                {
                    state_tx = C_RCV_CONTROL_tx;
                    printf("Recebi controlo no controlo\n");
                }
                else{
                    state_tx = START_tx;
                }
                break;
            case C_RCV_CONTROL_tx:
                if (buf_controlo[0] == BCC_CONTROL_tx)
                {
                    state_tx = BCC_OK_CONTROL_tx;
                    printf("Recebi BCC no controlo\n");
                }
                else{
                    state_tx = START_tx;
                }
                break;
            case BCC_OK_CONTROL_tx:
                if (buf_controlo[0] == FLAG)
                {
                    state_tx = END_CONTROL_tx;
                    printf("Recebi 2ª flag no controlo\n");
                    printf("\nLigação estabelecida com sucesso\n");      
                }
                else{
                    state_tx = START_tx;
                }
                break;
    
            default:
                break;
        }
    }
    printf("\nChHEGOU AQUI\n");
    return alarmCounter;
}

int establish_connection_rx() {

    state_rx = START_rx;

    printf("\nA tentar estabelecer conexão\n");

    while (state_rx != END_CONTROL_rx)
    {
        res_control = read(fd,buf_controlo,1);   /* returns after 5 chars have been input */
        buf_controlo[res_control]=0; 
        switch (state_rx){
            case START_rx:
                if( buf_controlo[0] == FLAG){
                    state_rx = FLAG_RCV_rx;
                    printf("\nRecebi 1ª flag no controlo\n");
                }
                else{
                    state_rx = START_rx;
                }
                break;
            case FLAG_RCV_rx:
                if (buf_controlo[0] == FLAG){
                    state_rx = FLAG_RCV_rx;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if( buf_controlo[0] == A_SENDER){
                    state_rx = A_RCV_rx;
                    printf("Recebi acknowledge no controlo\n");
                }                
                else{
                    state_rx = START_rx;
                }
                break;
            case A_RCV_rx:
                if (buf_controlo[0] == FLAG){
                    state_rx = FLAG_RCV_rx;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if (buf_controlo[0] == C_CONTROL_SENDER)
                {
                    state_rx = C_RCV_CONTROL_rx;
                    printf("Recebi controlo no controlo\n");
                }
                else{
                    state_rx = START_rx;
                }
                break;
            case C_RCV_CONTROL_rx:
                if (buf_controlo[0] == FLAG){
                    state_rx = FLAG_RCV_rx;
                    printf("Recebi flag quando não devia no controlo\n");
                }
                if (buf_controlo[0] == BCC_CONTROL_rx)
                {
                    state_rx = BCC_OK_CONTROL_rx;
                    printf("Recebi bcc no controlo\n");
                }
                else{
                    state_rx = START_rx;
                }
                break;
            case BCC_OK_CONTROL_rx:
                if (buf_controlo[0] == FLAG)
                {
                    state_rx = END_CONTROL_rx;
                    printf("Recebi 2ª flag no controlo\n");
                    printf("\nChHEGOU AQUI\n");
                    printf("\nTrama de controlo recebida com sucesso\n");
                }
                else{
                    state_rx = START_rx;              
                }
                break;
            default:
                break;
        }
    }

    if (state_rx == END_CONTROL_rx)
    {
        printf("\nA enviar feedback de controlo");

        buf_controlo[0] = FLAG;
        buf_controlo[1] = A_RECEIVER;
        buf_controlo[2] = C_CONTROL_RECEIVER;
        buf_controlo[3] = buf_controlo[1]^buf_controlo[2];
        buf_controlo[4] = FLAG;

        res_control = write(fd,buf_controlo,5);
        printf("\n%d Control bytes written\n", res_control);
        return 1;
        /*for (int x = 0; x < 5; x++){
            printf("buf_controlo[%d]: %x\n", x, buf_controlo[x]);
        }*/

    }
}


void send_rr_rx(){

    if (state_rx == END_INFO_rx)
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

        printf("\n%d Info bytes written\n", res_info_rr);
        printf("Trama RR enviada com sucesso\n");
    }
}

void termination_tx(){

    if (state_tx == END_INFO_tx){
        printf("\nVou enviar trama de terminação");

        buf_disc[0] = FLAG;
        buf_disc[1] = A_SENDER;
        buf_disc[2] = C_DISC;
        buf_disc[3] = buf_disc[1]^buf_disc[2];
        buf_disc[4] = FLAG;

        res_disc = write(fd,buf_disc,5);
        printf("\n%d Disc bytes written\n", res_disc);
    }

    if (state_tx == END_INFO_tx)
    {
       printf("\nA receber feedback de terminação\n");
       while (state_tx != END_DISC_tx)
        {
            res_disc = read(fd,buf_disc,1);   /* returns after 5 chars have been input */
            buf_disc[res_disc]=0; 
            switch (state_tx){
                case END_INFO_tx:
                    if (buf_disc[0] == FLAG){
                        state_tx = FLAG_RCV_DISC_tx;
                        printf("\nRecebi 1ª flag disc\n");
                    }
                    else{
                        state_tx = END_INFO_tx;
                    }
                    break;
                case FLAG_RCV_DISC_tx:
                    if (buf_disc[0] == A_RECEIVER){
                        state_tx = A_RCV_DISC_tx;
                        printf("Recebi acknowledge disc\n");
                    }
                    else{
                        state_tx = END_INFO_tx;
                    }
                    break;
                case A_RCV_DISC_tx:
                    if (buf_disc[0] == C_DISC){
                        state_tx = C_RCV_DISC_tx;
                        printf("Recebi controlo disc\n");
                    }
                    else{
                        state_tx = END_INFO_tx;
                    }
                    break;
                case C_RCV_DISC_tx:
                    if (buf_disc[0] == BCC_DISC_tx){
                        state_tx = BCC_OK_DISC_tx;
                        printf("Recebi BCC1 disc\n");
                    }
                    else{
                        state_tx = END_INFO_tx;
                    }
                    break;
                case BCC_OK_DISC_tx:
                    if (buf_disc[0] == FLAG){
                        state_tx= END_DISC_tx;
                        printf("Recebi 2ª flag disc\n");
                        printf("\nTrama de terminação recebida com sucesso\n");
                    }
                    else{
                        state_tx = END_INFO_tx;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (state_tx == END_DISC_tx)
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

void termination_rx(){

    printf("\nA receber trama de terminação\n");

    if (state_rx == END_INFO_rx)
    {
       while (state_rx != END_DISC_rx)
        {
            res_disc = read(fd,buf_disc,1);   /* returns after 5 chars have been input */
            buf_disc[res_disc]=0; 
            switch (state_rx){
                case END_INFO_rx:
                    if (buf_disc[0] == FLAG){
                        state_rx = FLAG_RCV_DISC_rx;
                        printf("\nRecebi 1ª flag disc\n");
                    }
                    else{
                        state_rx = END_INFO_rx;
                    }
                    break;
                case FLAG_RCV_DISC_rx:
                    if (buf_disc[0] == A_SENDER){
                        state_rx = A_RCV_DISC_rx;
                        printf("Recebi acknowledge disc\n");
                    }
                    else{
                        state_rx = END_INFO_rx;
                    }
                    break;
                case A_RCV_DISC_rx:
                    if (buf_disc[0] == C_DISC){
                        state_rx = C_RCV_DISC_rx;
                        printf("Recebi controlo disc\n");
                    }
                    else{
                        state_rx = END_INFO_rx;
                    }
                    break;
                case C_RCV_DISC_rx:
                    if (buf_disc[0] == BCC_DISC_rx){
                        state_rx = BCC_OK_DISC_rx;
                        printf("Recebi BCC1 disc\n");
                    }
                    else{
                        state_rx = END_INFO_rx;
                    }
                    break;
                case BCC_OK_DISC_rx:
                    if (buf_disc[0] == FLAG){
                        state_rx = END_DISC_rx;
                        printf("Recebi 2ª flag disc\n");
                        printf("\nTrama de terminação recebida com sucesso\n");
                    }
                    else{
                        state_rx = END_INFO_rx;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (state_rx == END_DISC_rx){

        printf("\nA enviar feedback DISC");

        buf_disc[0] = FLAG;
        buf_disc[1] = A_RECEIVER;
        buf_disc[2] = C_DISC;
        buf_disc[3] = buf_disc[1]^buf_disc[2];
        buf_disc[4] = FLAG;

        res_disc = write(fd,buf_disc,5);
        printf("\n%d Disc bytes written\n", res_disc);
    }

    if (state_rx == END_DISC_rx)
    {
        printf("\nA terminar ligação\n");

        while (state_rx != END_UA_rx)
        {
            res_discua_r = read(fd,buf_disc_ua,1);   
            buf_disc_ua[res_discua_r]=0; 
            switch (state_rx){
                case END_DISC_rx:
                    if (buf_disc_ua[0] == FLAG){
                        state_rx = FLAG_RCV_UA_rx;
                        printf("\nRecebi 1ª flag UA\n");
                    }
                    else{
                        state_rx = END_DISC_rx;
                    }
                    break;
                case FLAG_RCV_UA_rx:
                    if (buf_disc_ua[0] == A_SENDER){
                        state_rx = A_RCV_UA_rx;
                        printf("Recebi acknowledge UA\n");
                    }
                    else{
                        state_rx = END_DISC_rx;
                    }
                    break;
                case A_RCV_UA_rx:
                    if (buf_disc_ua[0] == C_CONTROL_RECEIVER){
                        state_rx = C_RCV_UA_rx;
                        printf("Recebi controlo UA\n");
                    }
                    else{
                        state_rx = END_DISC_rx;
                    }
                    break;
                case C_RCV_UA_rx:
                    if (buf_disc_ua[0] == BCC_CONTROL_rx){
                        state_rx = BCC_OK_UA_rx;
                        printf("Recebi BCC2 UA\n");
                    }
                    else{
                        state_rx = END_DISC_rx;
                    }
                    break;
                case BCC_OK_UA_rx:
                    if (buf_disc_ua[0] == FLAG){
                        state_rx = END_UA_rx;
                        printf("Recebi 2ª flag UA\n");

                        printf("\nLIGAÇÃO TERMINADA\n");
                    }
                    else{
                        state_rx = END_DISC_rx;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

int llopen(linkLayer connectionParameters) {

    connection = connectionParameters;
    fd = open(connection.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0){ 
        perror(connection.serialPort); 
        return -1; 
    }

    if (tcgetattr(fd, &oldtio) == -1){ 
        perror("tcgetattr"); 
        return -1; 
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connection.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;   
    newtio.c_cc[VMIN] = 0.1;   

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1){ 
        perror("tcsetattr"); 
        return -1; 
    }

    unsigned char byte; 
    timeout = connection.timeOut;
    retrans_data_counter = connection.numTries;
    switch(connection.role) { 
        case TRANSMITTER:{
            establish_connection_tx();
            
            break;
        }
        case RECEIVER:{
            establish_connection_rx();
            break;
        }
        default: 
            return -1; 
            break; 
    }
    return 1;
}

int llwrite(unsigned char *buf, int bufSize){
    
    unsigned char buf_info_tx[2*MAX_PAYLOAD_SIZE];
    int trama_info_size = 0;
    unsigned char bcc2_aux;
    int flag = TRUE;

    printf("\nVou começar a enviar trama de informação");

    buf_info_tx[0] = FLAG;
    buf_info_tx[1] = A_SENDER;
        
    if(s % 2 == 0){
        buf_info_tx[2] = S0;
        s++;
    }
    else{
        buf_info_tx[2] = S1;
        s++;
    }
    buf_info_tx[3] = buf_info_tx[1]^buf_info_tx[2];


    for ( i = 0; i < bufSize ; i++)
    {
        bcc2_aux ^= buf[i];

        if (buf[i] == FLAG)
        {
            printf("\nTEVE FLAG NOS DADOS\n");
            buf_info_tx[4+trama_info_size] = 0x5d;
            buf_info_tx[5+trama_info_size] = 0x7c;
            trama_info_size+= 2;
        }
        if (buf[i] == 0x5d)
        {
            printf("\nESC NOS DADOS\n");
            buf_info_tx[4+trama_info_size] = 0x5d;
            buf_info_tx[5+trama_info_size] = 0x7d;
            trama_info_size+= 2;
        }
        else {
            buf_info_tx[4+trama_info_size] = buf[i];
            trama_info_size++;
        }

    }
    if (bcc2_aux == FLAG)
    {
        buf_info_tx[4+trama_info_size] = 0x5d;
        buf_info_tx[5+trama_info_size] = 0x7c;
        buf_info_tx[6+trama_info_size] = FLAG;
    }
    else if (bcc2_aux == 0x5d)
    {
        buf_info_tx[4+trama_info_size] = 0x5d;
        buf_info_tx[5+trama_info_size] = 0x7d;
        buf_info_tx[6+trama_info_size] = FLAG;
    }else{
        buf_info_tx[4+trama_info_size] = bcc2_aux;
        buf_info_tx[5+trama_info_size] = FLAG;
    }
    trama_info_size += 6;
    
    //printf("\nBCC2: %x\n", bcc2_aux);

    while(state_tx != END_INFO_tx){

       if(flag){
        res_info = write(fd,buf_info_tx,trama_info_size);
        printf("\n%d Info bytes written\n", res_info);
        state_tx = SEND_INFO_tx;
        flag = FALSE;
        } 
        
       
        printf("\nA receber feedback RR\n");
         
        printf("state= %d\n", state_tx);
        res_info_rr = read(fd,buf_info_rr,1);
        printf("res_info_rr= %d\n", buf_info_rr[0]);

        switch (state_tx){
            case SEND_INFO_tx:
                if (buf_info_rr[0] == FLAG)
                    {
                        state_tx = FLAG_RCV_INFO_tx;
                        printf("\nRecebi 1ª flag RR\n");
                       
                    }
                    else{
                        state_tx == SEND_INFO_tx;
                        
                    }                
                    break;
                case FLAG_RCV_INFO_tx:
                    if (buf_info_rr[0] == A_RECEIVER)
                    {
                        state_tx = A_RCV_INFO_tx;
                        printf("Recebi acknowledge RR\n");
                        
                    }
                    else{
                        state_tx == SEND_INFO_tx;
                        
                    }
                    break;
                case A_RCV_INFO_tx:
                    if (buf_info_rr[0] == RR0)
                    {
                        state_tx = S_C_RCV_INFO_tx;
                        printf("Recebi RR0\n");
                        
                    }
                    else if (buf_info_rr[0] == RR1)
                    {
                        state_tx = S_C_RCV_INFO_tx;
                        printf("Recebi RR1\n");
                        
                    }
                    else if (buf_info_rr[0] == REJ0 || REJ1)
                    {
                        state_tx = END_CONTROL_tx;
                        printf("Recebi REJ\n");
                        flag = TRUE;
                        
                    }
                    else{
                        state_tx == SEND_INFO_tx;
                       
                    }
                    break;
                case S_C_RCV_INFO_tx:
                    //printf("RR: %x\n", buf_info_rr[0]);
                    if (buf_info_rr[0] == (A_RECEIVER^RR0))
                    {
                        state_tx = S_BCC_OK_1_INFO_tx;
                        printf("Recebi BCC RR0\n");
                        
                    }
                    else if (buf_info_rr[0] == (A_RECEIVER^RR1))
                    {                        
                        state_tx = S_BCC_OK_1_INFO_tx;
                        printf("Recebi BCC RR1\n");
                        
                    }
                    else{
                        state_tx == SEND_INFO_tx;
                        
                    }
                    break;
                case S_BCC_OK_1_INFO_tx:
                    if (buf_info_rr[0] == FLAG)
                    {
                        state_tx = END_INFO_tx;
                        printf("Recebi 2ª flag RR\n");
                        printf("\nInformação enviada com sucesso\n");
                       
                    }
                    else{
                        state_tx == SEND_INFO_tx;
                       
                    }
                    break;

            }           
    }   

    return trama_info_size;

}
        

int llread(unsigned char *packet){

    unsigned char buf_info_rx[2*MAX_PAYLOAD_SIZE];
    unsigned char buf_aux[2 * MAX_PAYLOAD_SIZE]; // Buffer for storing the initial packet data.
    unsigned char stored_data[MAX_PAYLOAD_SIZE]; // Buffer for storing the initial packet data.
    unsigned char buf_info_rr[6]; // Buffer for constructing response frame.

    int bcc2_cal = 0;
    int countRetries = 0;

    while (countRetries <= retrans_data_counter)
    {
        bcc2_aux = pos = 0;
        memset(buf_info_rx, 0, sizeof(buf_info_rx)); // buffer with the data from the serial port
        memset(stored_data, 0, sizeof(stored_data)); // buffer with the data from the serial port
        memset(buf_info_rr, 0, sizeof(buf_info_rr)); // Initialize the response frame buffer.

        printf("\nA receber trama de informação\n");

        while (state_rx != END_INFO_rx) {
            prev_res_info = res_info;
            res_info = read(fd, buf_info_rx + pos, 1); 
 
            switch (state_rx) {
                case END_CONTROL_rx:
                    if (buf_info_rx[pos] == FLAG) {
                        state_rx = FLAG_RCV_INFO_rx;
                        pos++;
                        printf("\nRecebi 1ª flag info\n");
                    } else {
                        state_rx = END_CONTROL_rx;
                    }
                    break;
                case FLAG_RCV_INFO_rx:
                    if (buf_info_rx[pos] == A_SENDER) {
                        state_rx = A_RCV_INFO_rx;
                        pos++;
                        printf("Recebi acknowledge info\n");
                    } else {
                        state_rx = END_CONTROL_rx;
                    }
                    break;
                case A_RCV_INFO_rx:
                    if (buf_info_rx[pos] == S0) {
                        s = 0;
                        pos++;
                        state_rx = S_C_RCV_INFO_rx;
                        printf("Recebi controlo info\n");
                    } else if (buf_info_rx[pos] == S1) {
                        s = 1;
                        pos++;
                        state_rx = S_C_RCV_INFO_rx;
                        printf("Recebi controlo info\n");
                    } else {
                        state_rx = END_CONTROL_rx;
                    }
                    break;
                case S_C_RCV_INFO_rx:
                    if ((buf_info_rx[pos] == (A_SENDER ^ S0)) || (buf_info_rx[pos] == (A_SENDER ^ S1))) {
                        state_rx = S_BCC_OK_1_INFO_rx;
                        pos++;
                        printf("Recebi BCC1 info\n");
                    } else {
                        state_rx = END_CONTROL_rx;
                    }
                    break;
                case S_BCC_OK_1_INFO_rx:
                    if((buf_info_rx[pos] == FLAG)){
                        state_rx = END_INFO_rx;
                        printf("Recebi BCC2 info\n");
                        printf("\nRecebi toda a trama info\n");
                    }
                    else pos++;
                    break;    
            }      
        }
    
        if(buf_info_rx[pos - 3] == 0x5d && buf_info_rx[pos - 2] == 0x7c){ bcc2_aux = FLAG; pos--;}   
        else if(buf_info_rx[pos - 3] == 0x5d && buf_info_rx[pos - 2] == 0x7d){ bcc2_aux = 0x5d; pos--;}  
        else    bcc2_aux = buf_info_rx[pos - 3];
        
        //put only the data in the packet
        for (int j = 0; j < pos - 6; j++) {
        stored_data[j] = buf_info_rx[4 + j];
        }

        int j = 0;
        for(int i = 0; i < pos - 6; i++) {
            if(stored_data[i] == 0x5d && stored_data[i + 1] == 0x7c) {
                packet[j] = 0x5c; j++;
                i++;
            } else if(stored_data[i] == 0x5d && stored_data[i + 1] == 0x7d) {
                packet[j] = 0x5d; j++;
                i++;
            } else {packet[j] = stored_data[i]; j++;}
        }

        for (int i = 0; i < j; i++) bcc2_cal ^= packet[i];

        memset(buf_info_rr, 0, sizeof(buf_info_rr)); // Initialize the response frame buffer.
        buf_info_rr[0] = FLAG;
        buf_info_rr[1] = A_RECEIVER;
        buf_info_rr[4] = FLAG;

        // Check if the BCC is correct
        if(bcc2_cal == bcc2_aux){
            s = (s + 1) % 2;
            if(s)  buf_info_rr[2] = RR1;
            else    buf_info_rr[2] = RR0;

            buf_info_rr[3] = buf_info_rr[1] ^ buf_info_rr[2];
            if(write(fd, buf_info_rr, 5) < 0) {
                printf("Error sending RR.\n");
                return -1;
            }
            printf("Acknowledgement frame sent.\n");

            return j;// Return the size of the received packet.
                
        }else if(bcc2_cal != bcc2_aux){
            if(s) buf_info_rr[2] = REJ1;
            else   buf_info_rr[2] = REJ0;

            printf("Bcc_cal: %02x and bcc2_aux: %02x\n", bcc2_cal, bcc2_aux);

            printf("Bad packet detected.\n");
            buf_info_rr[3] = buf_info_rr[1] ^ buf_info_rr[2];
            write(fd, buf_info_rr, 5);

            countRetries++;

        }
    }

    return -1;
}

int llclose(linkLayer connection, int showStats) {
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) { perror("tcsetattr"); return -1; }

    close(fd);
    return 1;
}