#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <netdb.h>

#include "dhcp.h"
#include "format.h"
#include "port_utils.h"

static char* client_addrs[10] = {"10.0.2.1", "10.0.2.2", "10.0.2.3", "10.0.2.4", "10.0.2.5", 
                                "10.0.2.6", "10.0.2.7", "10.0.2.8", "10.0.2.9", "10.0.2.10"};
int next_addr = 0;

void run_server ();
void print_server_received (dhcp_msg_t *);
void print_server_sending (dhcp_msg_t *);
void setup_dhcp (dhcp_msg_t *, dhcp_msg_t *, int, struct in_addr *,
                  struct in_addr *, struct in_addr *, bool);

int main(int argc, char **argv)
{
  run_server ();
  return EXIT_SUCCESS;
}

void
run_server ()
{
  // set up server

  struct sockaddr_in addr;
  socklen_t addr_size = sizeof (addr);

  int sockfd = socket (AF_INET, SOCK_DGRAM, 0);
  assert (sockfd >= 0);
  struct timeval tv = {2, 0};
  setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
  int sock_option = 1;
  setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_option, sizeof (int));

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;

  char port_str[10];
  memset (port_str, 0, sizeof (port_str));
  memcpy (port_str, get_port (), sizeof (port_str));
  char *ptr;
  uint16_t port = strtol (port_str, &ptr, 10);
  addr.sin_port = htons (port);

  addr.sin_addr.s_addr = inet_addr ("127.0.0.1");

  assert (bind (sockfd, (struct sockaddr *)&addr, addr_size) >= 0);
  
  // send and receive from client

  char *buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
  int extra;

  struct in_addr server;
  inet_aton ("10.0.2.0", &server);

  while (recvfrom (sockfd, buffer, sizeof (dhcp_msg_t), 0,
          (struct sockaddr *) &addr, &addr_size) >= 0)
    {
      // copy received bytes into dhcp struct
      dhcp_msg_t dhcp;
      memset (&dhcp, 0, sizeof (dhcp));
      memcpy (&dhcp, buffer, sizeof (dhcp));
      dhcp.msg.xid = switch_endianness_32 (dhcp.msg.xid);
      print_server_received (&dhcp);

      switch (get_dhcp_type (&dhcp))
        {

          case DHCPDISCOVER:
            // send DHCPOFFER if < 10 client_addrs have been claimed
            if (next_addr < 10)
              {
                dhcp_msg_t offer;
                memset (&offer, 0, sizeof (dhcp_msg_t));
                struct in_addr yiaddr;
                inet_aton (client_addrs[next_addr], &yiaddr);
                setup_dhcp (&offer, &dhcp, DHCPOFFER, &yiaddr, &server, &yiaddr, true);
                char *offer_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
                memcpy (offer_buffer, &offer, sizeof (offer));
                extra = calculate_extra_bytes (&offer);
                print_server_sending (&offer);
                offer.msg.xid = switch_endianness_32 (offer.msg.xid);
                assert (sendto (sockfd, offer_buffer, sizeof(msg_t) + extra, 0, 
                        (struct sockaddr *)&addr, addr_size) >= 0);
                free (offer_buffer);
                next_addr++;
              }
            // send DHCPNAK if all client_addrs are claimed
            else
              {
                dhcp_msg_t nak;
                memset (&nak, 0, sizeof (dhcp_msg_t));
                struct in_addr yiaddr;
                inet_aton ("0.0.0.0", &yiaddr);
                setup_dhcp (&nak, &dhcp, DHCPNAK, &yiaddr, &server, NULL, false);
                char *nak_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
                memcpy (nak_buffer, &nak, sizeof (dhcp_msg_t));
                extra = calculate_extra_bytes (&nak);
                print_server_sending (&nak);
                nak.msg.xid = switch_endianness_32 (nak.msg.xid);
                assert (sendto (sockfd, nak_buffer, sizeof(msg_t) + extra, 0, 
                        (struct sockaddr *)&addr, addr_size) >= 0);
                free (nak_buffer);
              }
            break;

          case DHCPREQUEST:
            ; // empty statement for compiler warnings
            dhcp_msg_t ack;
            memset (&ack, 0, sizeof (dhcp_msg_t));
            struct in_addr request;
            // pull requested address from client
            uint32_t request_num;
            for (int i = 0; i < sizeof (dhcp.options.opt); i++) 
              {
                if (dhcp.options.opt[i] == 0x32)
                  {
                    request_num = process_int (dhcp.options.opt[i+2], dhcp.options.opt[i+3], 
                                                dhcp.options.opt[i+4], dhcp.options.opt[i+5]);
                  }
              }
            request.s_addr = request_num;
            setup_dhcp (&ack, &dhcp, DHCPACK, &request, &server, NULL, true);
            char *ack_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
            memcpy (ack_buffer, &ack, sizeof (dhcp_msg_t));
            extra = calculate_extra_bytes (&ack);
            print_server_sending (&ack);
            ack.msg.xid = switch_endianness_32 (ack.msg.xid);
            assert (sendto (sockfd, ack_buffer, sizeof(msg_t) + extra, 0, 
                        (struct sockaddr *)&addr, addr_size) >= 0);
            free (ack_buffer);
            break;

          default:
            break;
        }

      // clear buffer
      memset (buffer, 0, sizeof (dhcp_msg_t));
    }
    free (buffer);
    close (sockfd);
}

void
print_server_received (dhcp_msg_t *dhcp)
{
  printf ("++++++++++++++++\n");
  printf ("SERVER RECEIVED:\n");
  print_dhcp (dhcp);
  printf ("++++++++++++++++\n");
}

void
print_server_sending (dhcp_msg_t *dhcp)
{
  printf ("SERVER SENDING:\n");
  print_dhcp (dhcp);
}

// add request field
void
setup_dhcp (dhcp_msg_t *reply, dhcp_msg_t *req, int type, struct in_addr *yiaddr, 
            struct in_addr *server, struct in_addr *request, bool include_time)
{
  reply->msg.op = BOOTREPLY;
  reply->msg.htype = req->msg.htype;
  reply->msg.hlen = req->msg.hlen;
  reply->msg.xid = req->msg.xid;
  for (int i = 0; i < sizeof (reply->msg.chaddr); i++)
    reply->msg.chaddr[i] = req->msg.chaddr[i];
  reply->msg.yiaddr = *yiaddr;
  reply->options.cookie = switch_endianness_32(MAGIC_COOKIE);
  uint32_t lease_time = 0x008d2700;
  if (include_time)
    set_dhcp_options (reply, type, server, request, lease_time);
  else
    set_dhcp_options (reply, type, server, request, 0);
}