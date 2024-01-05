#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include "dhcp.h"
#include "format.h"
#include "port_utils.h"
#include "dhcp.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>

void initiate_protocol (dhcp_msg_t *);

int main (int argc, char **argv)
{
  // Default values
  dhcp_msg_t dhcp;
  memset (&dhcp, 0, sizeof (dhcp));
  dhcp.msg.op = BOOTREQUEST;
  dhcp.msg.htype = ETH;
  dhcp.msg.hlen = ETH_LEN;
  dhcp.msg.xid = 42;
  for (int i = 0; i < 8; i++)
    dhcp.msg.chaddr[i] = i+1;
  dhcp.options.cookie = switch_endianness_32 (MAGIC_COOKIE);
  int dhcp_opt = DHCPDISCOVER;
  set_dhcp_options (&dhcp, DHCPDISCOVER, NULL, NULL, 0);
  
  struct in_addr server;
  inet_aton ("127.0.0.1", &server);

  struct in_addr request;
  inet_aton ("127.0.0.2", &request);

  bool protocol = false;

  // Parse command-line arguments
    
  for (int i = 0; i < argc; i++) 
    {
      // set xid
      if (strcmp(argv[i], "-x") == 0)
        {
          dhcp.msg.xid = (uint32_t)strtoul(argv[i + 1], NULL, 10);
        }
      // set hardware type
      else if (strcmp(argv[i], "-t") == 0)
        {
          dhcp.msg.htype = (uint8_t)strtoul(argv[i + 1], NULL, 10);
        }
      // set hardware address
      else if (strcmp(argv[i], "-c") == 0)
        {
          char str[strlen (argv[i+1]) + 1];
          memset (str, 0, sizeof (str));
          snprintf (str, sizeof (str), "%s", argv[i+1]);
          for (int i = 0; i < sizeof (str); i+=2)
            {
              char substr[3];
              memset (substr, 0, sizeof (substr));
              for (int j = 0; j < 2; j++)
                substr[j] = str[i+j];
              char *ptr;
              dhcp.msg.chaddr[i/2] = strtoul (substr, &ptr, 16);
            }
        }
      // set DHCP message type
      else if (strcmp(argv[i], "-m") == 0)
        {
          if (i + 1 < argc && argv[i + 1][0] != '-')
            {
              dhcp_opt = strtoul(argv[i + 1], NULL, 10);
            }
        }
      // set server IP DHCP option
      else if (strcmp(argv[i], "-s") == 0)
        {
          inet_aton (argv[i+1], &server);
        }
      // set requested IP DHCP option
      else if (strcmp(argv[i], "-r") == 0)
        {
          inet_aton (argv[i+1], &request);
        }
      // initiate protocol
      else if (strcmp(argv[i], "-p") == 0)
        {
          protocol = true;
        }
    }

  // htype and hlen ifs
  switch (dhcp.msg.htype)
    {
      case ETH:
        dhcp.msg.hlen = ETH_LEN;
        break;
      case IEEE802:
        dhcp.msg.hlen = IEEE802_LEN;
        break;
      case ARCNET:
        dhcp.msg.hlen = ARCNET_LEN;
        break;
      case FRAME_RELAY:
        dhcp.msg.hlen = FRAME_LEN;
        break;
      case FIBRE:
        dhcp.msg.hlen = FIBRE_LEN;
        break;
      default:
        exit (1);
    }

  switch (dhcp_opt)
    {
      case DHCPDISCOVER:
        set_dhcp_options (&dhcp, dhcp_opt, NULL, NULL, 0);
        break;
      case DHCPOFFER:
        set_dhcp_options (&dhcp, dhcp_opt, &server, NULL, 0);
        break;
      case DHCPREQUEST:
        set_dhcp_options (&dhcp, dhcp_opt, &server, &request, 0);
        break;
      case DHCPDECLINE:
        set_dhcp_options (&dhcp, dhcp_opt, &server, &request, 0);
        break;
      case DHCPACK:
        set_dhcp_options (&dhcp, dhcp_opt, &server, NULL, 0);
        break;
      case DHCPNAK:
        set_dhcp_options (&dhcp, dhcp_opt, &server, NULL, 0);
        break;
      case DHCPRELEASE:
        set_dhcp_options (&dhcp, dhcp_opt, &server, NULL, 0);
        break;
      default:
        exit (1);
        break;
    }

    print_dhcp (&dhcp);
    fflush (stdout);

    if (protocol)
      initiate_protocol (&dhcp);

  return EXIT_SUCCESS;
}

void
initiate_protocol (dhcp_msg_t *dhcp)
{
  struct sockaddr_in addr;
  socklen_t addr_size = sizeof (addr);
  int extra;

  int sockfd = socket (AF_INET, SOCK_DGRAM, 0);
  assert (sockfd >= 0);
  struct timeval tv = {10, 0};
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

  assert (connect (sockfd, (struct sockaddr *)&addr, addr_size) >= 0);
  
  if (dhcp->msg.xid == 0) // server will print what it receives
    {
      // create a buffer with the dhcp header info
      extra = calculate_extra_bytes (dhcp);
      char *buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
      memcpy (buffer, dhcp, sizeof (*dhcp));
      
      // send  to server
      assert (send (sockfd, buffer, sizeof (msg_t) + extra, 0) >= 0);

      free (buffer);
    }
  else // full DHCP send/response
    {
      // send DHCPDISCOVER
      extra = calculate_extra_bytes (dhcp);
      char *disc_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
      memcpy (disc_buffer, dhcp, sizeof (*dhcp));
      assert (send (sockfd, disc_buffer, sizeof (msg_t) + extra, 0) >= 0);
      
      // receive DHCPOFFER (or DHCPNAK)
      dhcp_msg_t offer;
      memset (&offer, 0, sizeof (offer));
      char *offer_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
      assert (recv (sockfd, offer_buffer, sizeof (dhcp_msg_t), 0) >= 0);
      memcpy (&offer, offer_buffer, sizeof (offer));
      printf ("++++++++++++++++\n");
      printf ("CLIENT RECEIVED:\n");
      print_dhcp (&offer);
      printf ("++++++++++++++++\n");

      if (get_dhcp_type (&offer) == DHCPNAK)
        exit (0);

      // construct DHCPREQUEST
      dhcp_msg_t req;
      memset (&req, 0, sizeof (req));
      req.msg.op = BOOTREQUEST;
      req.msg.htype = dhcp->msg.htype;
      req.msg.hlen = dhcp->msg.hlen;
      req.msg.xid = offer.msg.xid;
      for (int i = 0; i < sizeof (req.msg.chaddr); i++)
        req.msg.chaddr[i] = dhcp->msg.chaddr[i];
      req.options.cookie = switch_endianness_32 (MAGIC_COOKIE);
      struct in_addr req_server;
      memset (&req_server, 0, sizeof (req_server));
      char str_server[16];
      memset (str_server, 0, sizeof (str_server));
      snprintf (str_server, sizeof (str_server), "%d.%d.%d.%d", 
                offer.options.opt[11], offer.options.opt[12],
                offer.options.opt[13], offer.options.opt[14]);
      inet_aton (str_server, &req_server);
      set_dhcp_options (&req, DHCPREQUEST, &req_server, &offer.msg.yiaddr, 0);

      // send DHCPREQUEST
      char *req_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
      memcpy (req_buffer, &req, sizeof (req));
      extra = calculate_extra_bytes (&req);
      assert (send (sockfd, req_buffer, sizeof (msg_t) + extra, 0) >= 0);
      print_dhcp (&req);

      // receive DHCP ack
      dhcp_msg_t ack;
      memset (&ack, 0, sizeof (ack));
      char *ack_buffer = (char *) calloc (sizeof (dhcp_msg_t), sizeof (char));
      assert (recv (sockfd, ack_buffer, sizeof (dhcp_msg_t), 0) >= 0);
      memcpy (&ack, ack_buffer, sizeof (ack));
      printf ("++++++++++++++++\n");
      printf ("CLIENT RECEIVED:\n");
      print_dhcp (&ack);
      printf ("++++++++++++++++\n");

      // free buffers
      free (disc_buffer);
      free (offer_buffer);
      free (req_buffer);
      free (ack_buffer);
    }

  close (sockfd);
}
