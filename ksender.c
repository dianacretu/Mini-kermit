#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

//Culori <3
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

char findSeq(char s) {
    s ++;
    return s % 64;
}

int main(int argc, char** argv) {

    printf("%s Wait...\n", KGRN);
    msg t;
    package p;
    data_send d;
    int nr_timeout = 0;
    msg* y;

    init(HOST, PORT);

    //Cream "Send Init"
    p.soh = 1;
    p.len = 255;
    p.seq = 0;
    p.type = 'S';
    d.maxl = 250;
    d.time = 5;
    d.npad = d.padc = 0;
    d.eol = 0x0D;
    d.qctl = d.qbin = d.chkt = d.rept = d.capa = d.r = 0;

    memset(p.data, 0, 250);
    memcpy(p.data, &d, 11);
    p.mark = d.eol;
    p.check = crc16_ccitt(&p, 254);
    memcpy(t.payload, &p, 258);
    t.len = 258;

    //Trimitem "Send Init"
    send_message(&t);

    //Asteptam ACK sau NAK pentru "SEND INIT"
    while (nr_timeout < 3) {
        if (nr_timeout == 3) {
                return -1;
        }
        y = receive_message_timeout(d.time * 1000);
        if (y == NULL) {

            //In caz de timeout se trimite acelasi mesaj
            printf("%s ERROR\n", KRED);
            nr_timeout ++;
            
        } 
        else {
            printf("%s[%s] Got ACK with sequence [%u]\n", KMAG, argv[0], y->payload[2]);
            //Se trimite pachetul cu SEQ + 1 daca se primeste ACK/NAK
            if ( y->payload[2] != t.payload[2] ) {
                    send_message(&t);
                    continue;
                }
            if (y->payload[3] == 'N') {
                //Se trimite pachetul cu SEQ + 1
                p.seq = findSeq(p.seq);
                p.check = crc16_ccitt(&p, 254);
                memcpy(t.payload, &p, 258);
                nr_timeout = 0;
            }
            else {
                if (y->payload[2] == t.payload[2]) {
                    break;
                }
            }
        }
        //Trimitem ori urmatorul pachet ori pachetul curent
        send_message(&t);
    }

    //Trimiterea tuturor fisierelor
    for (int i = 1; i < argc; i ++) {

        //Send pachetul "F"
        nr_timeout = 0;
        p.seq = findSeq(p.seq);
        memset(p.data, 0, 250);
        //Se copiaza titlul fisierului
        sprintf(p.data, argv[i]);
        p.type = 'F';
        p.check = crc16_ccitt(&p, 254);
        memcpy(t.payload, &p, 258);
        send_message(&t);

        //Asteptare ACK sau NAK pt "F"
        while (nr_timeout < 3) {
            if (nr_timeout == 3) {
                return -1;
            }
            y = receive_message_timeout(d.time * 1000);
            if (y == NULL) {
                //In caz de timeout se trimite acelasi mesaj
                printf("%s ERROR\n", KRED);
                nr_timeout ++;    
            } 
            else {
                printf("%s[%s] Got reply with sequence [%u]\n", KMAG, argv[0], y->payload[2]);
                if ( y->payload[2] != t.payload[2] ) {
                    send_message(&t);
                    continue;
                }
                if (y->payload[3] == 'N') {
                    //Se trimite pachetul cu SEQ + 1
                    p.seq = findSeq(p.seq);
                    p.check = crc16_ccitt(&p, 254);
                    memcpy(t.payload, &p, 258);
                    nr_timeout = 0;
                }
                else {
                    if (y->payload[2] == t.payload[2]) {
                        break;
                    }
                }
            }
            send_message(&t);
        }
        //Iesirea din while inseamna ca s-a primit ACK
        //Putem trimite date din fisierul al carui titlu s-a trimis anterior
        nr_timeout = 0;
        p.type = 'D';
        FILE *f = fopen(t.payload + 4, "r");
        if (f == NULL) {
            return -1;
        }

        //Citirea din fisier
        while(1) {
            p.seq = findSeq(p.seq);
            memset(p.data, 0, 250);
            int chunk = fread(p.data, 1, 250, f);
            //S-a ajuns la finalul fisierului
            if (chunk == 0) {
                p.len = 0;
                memset(p.data, 0, 250);
                p.check = crc16_ccitt(&p, 254);
                memcpy(t.payload, &p, 258);
                send_message(&t);
                fclose(f);
                break;
            }
            p.len = chunk;
            p.check = crc16_ccitt(&p, 254);
            memcpy(t.payload, &p, 258);
            //Se trimite portiunea din fisier
            send_message(&t);
            nr_timeout = 0;

            //Se asteapta confirmarea ca s-a primit in receiver ce trebuie
            while (nr_timeout < 3) {
                if (nr_timeout == 3) {
                    return -1;
                }
                y = receive_message_timeout(d.time * 1000);
                if (y == NULL) {
                    //In caz de timeout se trimite acelasi mesaj
                    printf("%s ERROR\n", KRED);
                    nr_timeout ++;
                } 
                else {
                    printf("%s[%s] Got reply with sequence [%u]\n", KMAG, argv[0], y->payload[2]);
                    //Se trimite pachetul cu SEQ + 1 daca se primeste ACK/NAK
                    if ( y->payload[2] != t.payload[2] ) {
                        send_message(&t);
                        continue;
                    }
                    if (y->payload[3] == 'N') {
                        //Se trimite pachetul cu SEQ + 1
                        p.seq = findSeq(p.seq);
                        p.check = crc16_ccitt(&p, 254);
                        memcpy(t.payload, &p, 258);
                        nr_timeout = 0;
                     }
                     else {
                        if (y->payload[2] == t.payload[2]) {
                            break;
                         }
                     }
                }
                send_message(&t);
            }
            nr_timeout = 0;
        }
        
        //Confirmare ca s-a primit sfarsitul fisierului
        nr_timeout = 0;
        while (nr_timeout < 3) {
            if (nr_timeout == 3) {
                 return -1;
            }
            y = receive_message_timeout(d.time * 1000);
            if (y == NULL) {
                //In caz de timeout se trimite acelasi mesaj
                printf("%s ERROR\n", KRED);
                nr_timeout ++;
            } 
            else {
                printf("%s[%s] Got reply with sequence [%u]\n", KMAG, argv[0], y->payload[2]);
                //Se trimite pachetul cu SEQ + 1 daca se primeste ACK/NAK
                if ( y->payload[2] != t.payload[2] ) {
                    send_message(&t);
                    continue;
                }
                if (y->payload[3] == 'N') {
                    //Se trimite pachetul cu SEQ + 1
                    p.seq = findSeq(p.seq);
                    p.check = crc16_ccitt(&p, 254);
                    memcpy(t.payload, &p, 258);
                    nr_timeout = 0;
                }
                else {
                    if (y->payload[2] == t.payload[2]) {
                        break;
                    }
                }
            }
            send_message(&t);
        }
        
        //End of file
        nr_timeout = 0;
        p.seq = findSeq(p.seq);
        memset(p.data, 0, 250);
        p.type = 'Z';
        p.check = crc16_ccitt(&p, 254);
        memcpy(t.payload, &p, 258);
        send_message(&t);

        //Asteptare ACK sau NAK pt END OF FILE
        while (nr_timeout < 3) {
            if (nr_timeout == 3) {
                return -1;
            }
            y = receive_message_timeout(d.time * 1000);
            if (y == NULL) {
                //In caz de timeout se trimite acelasi mesaj
                printf("%s ERROR\n", KRED);
                nr_timeout ++;
                
            } 
            else {
                printf("%s[%s] Got reply with sequence [%u]\n", KMAG, argv[0], y->payload[2]);
                //Se trimite pachetul cu SEQ + 1 daca se primeste ACK/NAK
                if ( y->payload[2] != t.payload[2] ) {
                    send_message(&t);
                    continue;
                }
                if (y->payload[3] == 'N') {
                    //Se trimite pachetul cu SEQ + 1
                    p.seq = findSeq(p.seq);
                    p.check = crc16_ccitt(&p, 254);
                    memcpy(t.payload, &p, 258);
                    nr_timeout = 0;
                }
                else {
                    if (y->payload[2] == t.payload[2]) {
                        break;
                    }
                }
            }
            send_message(&t);
        }
    }

    //END OF TRANSMISSION
    nr_timeout = 0;
    p.seq = findSeq(p.seq);
    memset(p.data, 0, 250);
    p.type = 'B';
    p.check = crc16_ccitt(&p, 254);
    memcpy(t.payload, &p, 258);
    send_message(&t);

    //Asteptare ACK sau NAK pt END OF TRANSMISSION
    while (nr_timeout < 3) {
        if (nr_timeout == 3) {
            return -1;
        }
        y = receive_message_timeout(d.time * 1000);
        if (y == NULL) {
            //In caz de timeout se trimite acelasi mesaj
            printf("%s ERROR\n", KRED);
            nr_timeout ++;    
        } 
        else {
            printf("%s[%s] Got reply with sequence [%u]\n", KMAG, argv[0], y->payload[2]);
            //Se trimite pachetul cu SEQ + 1 daca se primeste ACK/NAK
            if ( y->payload[2] != t.payload[2] ) {
                send_message(&t);
                continue;
            }
            if (y->payload[3] == 'N') {
                //Se trimite pachetul cu SEQ + 1
                p.seq = findSeq(p.seq);
                p.check = crc16_ccitt(&p, 254);
                memcpy(t.payload, &p, 258);
                nr_timeout = 0;
            }
            else {
                printf("%sTRANSMISSION ENDED! FINALLY!\n", KGRN);
                break;
            }
        }
        send_message(&t);
    }
    
    return 0;
}
