#include<stdio.h>
#include<string.h>	//strlen
#include<stdlib.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write

#include<pthread.h> //for threading , link with lpthread

#include "global.h"
#include "tcpserver.h"
#include "car.h"
#include "myarea.h"

int parse_servo_value ( char * str, int l, int * v )
{
  if ( l <= 0 )
    return 0;

  *v = atoi ( str );
  return 1;
}

int parse_servo_onoff ( char * str, int l, int * v )
{
  if ( l <= 0 )
    return 0;

  if (   ( 0==strncmp ( str, "on", l ) )
      || ( 0==strncmp ( str, "1", l ) )   )
  {
    *v = 1;
    return 1;
  }

  if (   ( 0==strncmp ( str, "off", l ) )
      || ( 0==strncmp ( str, "0", l ) )   )
  {
    *v = 0;
    return 1;
  }

  return 0;
}

void reset_stack ( struct cconn * cc )
{
  int i, ii;

  for (i=0;i<TSMP;i++)
  {
    cc->l[i] = 0;

    for ( ii=0;ii<TSMPL;ii++ )
      cc->speicher[i][ii] = ' ';
  }

  cc->curp = 0;
  cc->curpp = 0;
}


void parse_stack ( struct cconn * cc )
{
  int i,ii;
  char ret[200];
  int retlen = 0;

  if ( cc->curp>=1 )
  {
    if ( 0==strncmp ( cc->speicher[0], "help", 4 ) )
    {
      retlen = snprintf ( ret, 200, "Available commands:\n   servo set <channel> <value>\n   servo led <onoff> <mask>\n" );
    }

    if ( 0==strncmp ( cc->speicher[0], "servo", 5 ) )
    {

      if ( cc->curp>=2 )
      {
        if ( 0==strncmp(cc->speicher[1], "set", 3) )
        {
          if ( cc->curp>=4 )
          {
            int v;
            int ch;
            if (    parse_servo_value ( cc->speicher[2], cc->l[2], &ch  )
                &&  parse_servo_value ( cc->speicher[3], cc->l[3], &v )
               )
            {
							if (global_ctrl == CTRL_TCP )
							{
								switch ( ch )
								{
									case 0:
										global_area->myCar.set_accel_servo(v);
										break;
									case 1:
										global_area->myCar.set_steering_servo(v);
										break;
								}
							}
              retlen = snprintf ( ret, 200, "ok servo set %d %d \n", ch, v );
            }
          }
        } //servo

        if ( 0==strncmp(cc->speicher[1], "led", 3) )
        {
          if ( cc->curp>=4 )
          {
            int mask;
            int onoff;
            if (   parse_servo_onoff ( cc->speicher[2], cc->l[2], &onoff )
                && parse_servo_value ( cc->speicher[3], cc->l[3], &mask ) )
            {
							if(onoff)
							{
								global_area->leds |= mask & ((1<<0)|(1<<1)|(1<<2));
							} else {
								global_area->leds &= ~(mask & ((1<<0)|(1<<1)|(1<<2)));
							}
              retlen = snprintf ( ret, 200, "ok servo led %d %d \n", onoff, mask );
            }
          }
        } //led
      }
    } //servoboard
  }

  if ( retlen > 0 )
    write ( cc->fd, ret, retlen );
  else
    write ( cc->fd, "nok \n", 5 );

  reset_stack ( cc );
}

void addc ( struct cconn * cc, char c )
{
  int i,ii;
  if ( c == '\n' )
  {
    if ( cc->curpp > 0 )
    {
      cc->l[cc->curp] = cc->curpp;
      cc->curp++;
    }


    parse_stack ( cc );
  } else if ( c == ' ' ) {
    if ( cc->curp < (TSMP-1) )
    {
      cc->l[cc->curp] = cc->curpp;
      cc->curp++;
      cc->curpp = 0;
    }
  } else {
    if ( cc->curpp < (TSMPL-1) )
    {
      cc->speicher[cc->curp][cc->curpp] = c;
      cc->curpp++;
    }
  }


}

void *connection_handler(void * cconn)
{
  struct cconn * cc = (struct cconn*) cconn;
	//int sock = *(int*)socket_desc;
  char c;
  int ret;
  int br;
	
	//write(sock , message , strlen(message));

  reset_stack ( cc );

  while ( (br=read(cc->fd,&c,1)==1 ) )
  {
    //printf ( "read: %c\n", c );
    addc ( cc, c );
  } 

	printf("N: tcpserver: client disconnected\n" );

  close ( cc->fd );
	free(cc);
	
	return 0;
}

int tcpserver_start ( struct tcpserver * ts )
{
	int socket_desc , new_socket , c , *new_sock;
	struct sockaddr_in server , client;
	char *message;

  struct cconn * newcc;
	
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("E: tcpserver: Could not create socket");
    return -1;
	}
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( ts->port );
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		puts("E: tcpserver: bind failed");
		return -1;
	}
	
	//Listen
	listen(socket_desc , 3);
	
	printf ("N: tcpserver: I'm listening on port %d\n", ts->port );
	
	c = sizeof(struct sockaddr_in);
	while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		
		pthread_t sniffer_thread;

    newcc = (struct cconn*)malloc ( sizeof(struct cconn) );
    newcc->fd = new_socket;
		
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) newcc) < 0)
		{
			perror("E: tcpserver: Could not create thread for incoming connection");
			return 1;
		}
		
		puts("N: tcpserver: Connection accepted");
	}
	
	if (new_socket<0)
	{
		perror("E: tcpserver: accept failed");
		return -1;
	}
	
	return 0;
}


