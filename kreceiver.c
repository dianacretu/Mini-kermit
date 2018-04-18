#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

//Culori <3
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

//Verifica daca pachetul este corupt prin controlarea sumei de control
int check(msg *a)  {
    unsigned short crc = crc16_ccitt(a->payload, a->len - 4);
    unsigned short crc2 = (unsigned short) ((unsigned char) a->payload[254]);
    unsigned short crc3 = (unsigned short) ((unsigned char) a->payload[255]);
    crc3 = crc3 << 8;
    crc2 = crc2 + crc3;
    if (crc == crc2) {
        return 1;
    }
    else {
        return 0;
    }
}

int main(int argc, char** argv) {

    msg t;
    msg* rec;
    int nr_timeout = 0, wait_time;

    init(HOST, PORT);

    //Pachetul "Send Init"
    while (1) {
        rec = receive_message_timeout(5 * 1000 * 3);
        //Timeout
        if (rec == NULL ) {
            printf("%s ERROR\n", KRED);
            return -1;
        } 
        printf("%s[%s] Got msg with sequence [%u]\n", KCYN, argv[0], rec->payload[2]);

        //Verificare trimitere ACK sau NAK in functie de suma de control
        if (check(rec) == 1) {
            rec->payload[3] = 'Y';
            wait_time = rec->payload[5] * 1000;
            memcpy(t.payload, rec->payload, 258);
            send_message(&t);
            break;
        }
        else {
            memset(rec->payload + 4, '0', 250);
            rec->payload[3] = 'N';
            memcpy(t.payload, rec->payload, 258);
            send_message(&t);
        }
        t.len = 258;
    }

    FILE *fp = NULL;
    char *tmp = "";

    //Verificare ce s-a primit de la Sender pentru fisiere
    while(1) {
        nr_timeout = 0;
        while (1) {
            if (nr_timeout == 3) {
                printf("%s ERROR\n", KRED);
                return -1;
            }
            rec = receive_message_timeout(wait_time);
        
            if (rec == NULL ) {
                //Timeout
                printf("%s ERROR\n", KRED);
                nr_timeout ++;
            }
            else {
                printf("%s[%s] Got msg with sequence [%u]\n", KCYN, argv[0], rec->payload[2]);
                //Verificare trimitere ACK sau NAK
                if (check(rec) == 1) {
                    //Se iese din program cand s-a primit END OF TRANSMISSION
                    if (rec->payload[3] == 'B') {
                        t.payload[2] ++;
                        send_message(&t);
                        return 0;
                    }
                    //Se creeaza fisierele in care vom scrie
                    tmp = strdup("recv_");
                    strcat(tmp, rec->payload + 4);
                    fp = fopen(tmp, "w");
                    free(tmp);
                    rec->payload[3] = 'Y';
                    sprintf(rec->payload + 4, tmp);
                    memcpy(t.payload, rec->payload, 258);
                    //Confirmare ca s-a primit pachetul cu titlul corect
                    send_message(&t);
                    break;
                }
                else {
                    memset(rec->payload + 4, '0', 250);
                    rec->payload[3] = 'N';
                    memcpy(t.payload, rec->payload, 258);
                    nr_timeout = 0;
                    send_message(&t);
                }
            }
        }

        if (fp == NULL) {
            return -1;
        }

        //Scrierea in fisier
        while(1) {
            nr_timeout = 0;
            //Verificare daca s-a primit portiunea din fisier
            while (1) {
                if (nr_timeout == 3) {
                    return -1;
                }
                rec = receive_message_timeout(wait_time);
                //Timeout
                if (rec == NULL ) {
                    printf("%s ERROR\n", KRED);
                    nr_timeout ++;
                } 
                else {
                    printf("%s[%s] Got msg with sequence [%u]\n", KCYN, argv[0], rec->payload[2]);

                    //Verificare trimitere ACK sau NAK
                    if (check(rec) == 1) {
                        rec->payload[3] = 'Y';
                        memcpy(t.payload, rec->payload, 258);
                        break;
                    }
                    else {
                        memset(rec->payload + 4, '0', 250);
                        rec->payload[3] = 'N';
                        memcpy(t.payload, rec->payload, 258);
                        nr_timeout = 0;
                        send_message(&t);
                        nr_timeout = 0;
                    }
                }
            }

            //S-a ajuns la finalul fisierului
            if (rec->payload[1] == 0) {
                fclose(fp);
                send_message(&t);
                break;
            }
            
            fwrite(rec->payload + 4, 1, (unsigned char) (rec->payload[1]), fp);
            send_message(&t);
        }

        //Pachetul END OF FILE
        nr_timeout = 0;
        while (1) {
            if (nr_timeout == 3) {
                return -1;
            }
            rec = receive_message_timeout(wait_time);
        
            if (rec == NULL ) {
                //Timeout
                printf("%s ERROR\n", KRED);
                nr_timeout ++;

            }
            else {
                printf("%s[%s] Got msg with sequence [%u]\n", KCYN, argv[0], rec->payload[2]);
                //Verificare trimitere ACK sau NAK
                if (check(rec) == 1) {
                    rec->payload[3] = 'Y';
                    memcpy(t.payload, rec->payload, 258);
                    break;
                }
                else {
                    memset(rec->payload + 4, '0', 250);
                    rec->payload[3] = 'N';
                    memcpy(t.payload, rec->payload, 258);
                    nr_timeout = 0;
                    send_message(&t);
                }
            }
        }
        send_message(&t);
    }
}