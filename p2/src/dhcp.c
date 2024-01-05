#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "dhcp.h"

void
read_file (char *filename)
{
  // read into dhcp message struct
  int fd = open (filename, O_RDONLY);
  if (fd == -1)
      exit (1);
  dhcp_msg_t dhcp;
  read (fd, &dhcp, sizeof (dhcp));
  close (fd);
  
  // correct read errors
  dhcp.msg.xid = switch_endianness_32 (dhcp.msg.xid);
  dhcp.msg.secs = switch_endianness_16 (dhcp.msg.secs);
  dhcp.options.cookie = dhcp.options.cookie;

  // dump_packet (&dhcp, sizeof (dhcp));
  print_dhcp (&dhcp);
}

void
print_dhcp (dhcp_msg_t *dhcp)
{
  // process for printing (BOOTP)

  char *op_text;
  if (dhcp->msg.op == BOOTREQUEST)
    op_text = "BOOTREQUEST";
  else if (dhcp->msg.op == BOOTREPLY)
    op_text = "BOOTREPLY";
  else
    exit (1);
  
  char *htype_text;
  if (dhcp->msg.htype == ETH)
    htype_text = "Ethernet (10Mb)";
  else if (dhcp->msg.htype == IEEE802)
    htype_text = "IEEE 802 Networks";
  else if (dhcp->msg.htype == ARCNET)
    htype_text = "ARCNET";
  else if (dhcp->msg.htype == FRAME_RELAY)
    htype_text = "Frame Relay";
  else if (dhcp->msg.htype == FIBRE)
    htype_text = "Fibre Channel";
  else
    exit (1);

  char chaddr_text[32];
  memset (chaddr_text, 0, 32);
  char chstr[3];
  for (int i = 0; i < dhcp->msg.hlen; i++)
    {
      snprintf (chstr, sizeof (chstr), "%02x", dhcp->msg.chaddr[i]);
      strncat (chaddr_text, chstr, sizeof (chaddr_text));
    }

  uint32_t secs = dhcp->msg.secs;
  uint32_t days = secs / 86400;
  secs -= days * 86400;
  uint32_t hours = secs / 3600;
  secs -= hours * 3600;
  uint32_t mins = secs / 60;
  secs -= mins * 60;

  // print message struct (BOOTP)
  printf ("------------------------------------------------------\n");
  printf ("BOOTP Options\n");
  printf ("------------------------------------------------------\n");
  printf ("Op Code (op) = %d [%s]\n", dhcp->msg.op, op_text);
  printf ("Hardware Type (htype) = %d [%s]\n", dhcp->msg.htype, htype_text);
  printf ("Hardware Address Length (hlen) = %d\n", dhcp->msg.hlen);
  printf ("Hops (hops) = %d\n", dhcp->msg.hops);
  printf ("Transaction ID (xid) = %d (0x%x)\n", dhcp->msg.xid, dhcp->msg.xid);
  printf ("Seconds (secs) = %d Days, %d:%02d:%02d\n", days, hours, mins, secs);
  printf ("Flags (flags) = %d\n", dhcp->msg.flags);
  printf ("Client IP Address (ciaddr) = %s\n", inet_ntoa(dhcp->msg.ciaddr));
  printf ("Your IP Address (yiaddr) = %s\n", inet_ntoa(dhcp->msg.yiaddr));
  printf ("Server IP Address (siaddr) = %s\n", inet_ntoa(dhcp->msg.siaddr));
  printf ("Relay IP Address (giaddr) = %s\n", inet_ntoa(dhcp->msg.giaddr));
  printf ("Client Ethernet Address (chaddr) = %s\n", chaddr_text);

  if (dhcp->options.cookie == switch_endianness_32 (MAGIC_COOKIE))
    {
      // process for printing (DHCP)

      char *type_text;
      uint8_t type = dhcp->options.opt[2];
      if (type == DHCPDISCOVER)
        type_text = "DHCP Discover";
      else if (type == DHCPOFFER)
        type_text = "DHCP Offer";
      else if (type == DHCPREQUEST)
        type_text = "DHCP Request";
      else if (type == DHCPDECLINE)
        type_text = "DHCP Decline";
      else if (type == DHCPACK)
        type_text = "DHCP ACK";
      else if (type == DHCPNAK)
        type_text = "DHCP NAK";
      else if (type == DHCPRELEASE)
        type_text = "DHCP Release";
      else
        exit (1);

      // print message struct (DHCP)
      
      // all dhcp messages print this
      printf ("------------------------------------------------------\n");
      printf ("DHCP Options\n");
      printf ("------------------------------------------------------\n");
      printf ("Magic Cookie = [OK]\n");
      printf ("Message Type = %s\n", type_text);

      bool print_server = false;
      for (int i = 0; i < sizeof (dhcp->options.opt); i++)
        {
          switch (dhcp->options.opt[i])
            {
              case 0x32:
                print_request (dhcp);
                break;
              case 0x33:
                print_ip_lease (dhcp);
                break;
              case 0x36:
                print_server = true;
                break;
              default:
                break;
            }
        }
      if (print_server)
        print_server_id (dhcp);
    }

}

void
print_server_id (dhcp_msg_t *dhcp)
{
  uint32_t server_id;
  for (int i = 0; i < sizeof (dhcp->options.opt); i++) 
    {
      if (dhcp->options.opt[i] == 0x36)
        {
          server_id = process_int (dhcp->options.opt[i+2], dhcp->options.opt[i+3], 
                                    dhcp->options.opt[i+4], dhcp->options.opt[i+5]);
        }
    }
  struct in_addr server_identifier;
  server_identifier.s_addr = server_id;
  printf ("Server Identifier = %s\n", inet_ntoa (server_identifier));
}

void
print_ip_lease (dhcp_msg_t *dhcp)
{
  uint32_t secs;
  for (int i = 0; i < sizeof (dhcp->options.opt); i++) 
    {
      if (dhcp->options.opt[i] == 0x33)
        {
          secs = process_int (dhcp->options.opt[i+2], dhcp->options.opt[i+4], 
                              dhcp->options.opt[i+3], dhcp->options.opt[i+5]);
        }
    }
  uint32_t days = secs / 86400;
  secs -= days * 86400;
  uint32_t hours = secs / 3600;
  secs -= hours * 3600;
  uint32_t mins = secs / 60;
  secs -= mins * 60;
  printf ("IP Address Lease Time = %d Days, %d:%02d:%02d\n", days, hours, mins, secs);
}

void
print_request (dhcp_msg_t *dhcp)
{
  uint32_t request_num;
  for (int i = 0; i < sizeof (dhcp->options.opt); i++) 
    {
      if (dhcp->options.opt[i] == 0x32)
        {
          request_num = process_int (dhcp->options.opt[i+2], dhcp->options.opt[i+3], 
                                      dhcp->options.opt[i+4], dhcp->options.opt[i+5]);
        }
    }

  struct in_addr request;
  request.s_addr = request_num;
  printf ("Request = %s\n", inet_ntoa (request));
}

uint32_t
process_int (uint8_t one, uint8_t two, uint8_t three, uint8_t four)
{
  uint32_t processed = 0x000000;

  uint32_t first = one;
  processed = processed | first;

  uint32_t second = two;
  second = second << 8;
  processed = processed | second;

  uint32_t third = three;
  third = third << 16;
  processed = processed | third;

  uint32_t fourth = four;
  fourth = fourth << 24;
  processed = processed | fourth;

  return processed;
}

uint32_t 
switch_endianness_32 (uint32_t num)
{
  return ((num & 0xFF000000) >> 24) |
          ((num & 0x00FF0000) >> 8) |
          ((num & 0x0000FF00) << 8) |
          ((num & 0x000000FF) << 24);
}

uint16_t
switch_endianness_16 (uint16_t num)
{
  return ((num & 0xFF00) >> 8) |
          ((num & 0x00FF) << 8);
}

int
get_dhcp_type (dhcp_msg_t *dhcp)
{
  for (int i = 0; i < sizeof (dhcp->options.opt) - 2; i++)
    {
      if (dhcp->options.opt[i] == 0x35)
        {
          return dhcp->options.opt[i+2];
        }
    }
  return -1;
}

void
dump_packet (uint8_t *ptr, size_t size)
{
  size_t index = 0;
  while (index < size)
    {
      fprintf (stderr, " %02" PRIx8, ptr[index++]);
      if (index % 32 == 0)
        fprintf (stderr, "\n");
      else if (index % 16 == 0)
        fprintf (stderr, "  ");
      else if (index % 8 == 0)
        fprintf (stderr, " .");
    }
  if (index % 32 != 0)
    fprintf (stderr, "\n");
  fprintf (stderr, "\n");
}

void
set_dhcp_options (dhcp_msg_t *dhcp, int type, struct in_addr *server, 
                  struct in_addr *request, uint32_t lease)
{
  uint32_t serv;
  uint32_t req;
  uint32_t lea;

  dhcp->options.cookie = switch_endianness_32 (MAGIC_COOKIE);
  
  dhcp->options.opt[0] = 0x35;
  dhcp->options.opt[1] = 0x01;
  dhcp->options.opt[2] = type;
  
  int num_opts = 0x0;

  if (server != NULL)
    num_opts = num_opts | 0x1; // 0001
  if (request != NULL)
    num_opts = num_opts | 0x2; // 0010
  if (lease != 0)
    num_opts = num_opts | 0x4; // 0100
  
  switch (num_opts)
    {
      case 0x0: // no server, request, or lease
        dhcp->options.opt[3] = 0xff;
        break;
      case 0x1: // 0001, only server
        dhcp->options.opt[3] = 0x36;
        dhcp->options.opt[4] = 0x04;
        serv = server->s_addr;
        dhcp->options.opt[5] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[6] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[7] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[8] = serv & 0xFF;
        dhcp->options.opt[9] = 0xff;
        break;
      case 0x2: // 0010, only request
        dhcp->options.opt[3] = 0x32;
        dhcp->options.opt[4] = 0x04;
        req = request->s_addr;
        dhcp->options.opt[5] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[6] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[7] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[8] = req & 0xFF;
        dhcp->options.opt[9] = 0xff;
        break;
      case 0x3: // 0011, server and request
        dhcp->options.opt[3] = 0x36;
        dhcp->options.opt[4] = 0x04;
        serv = server->s_addr;
        dhcp->options.opt[5] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[6] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[7] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[8] = serv & 0xFF;
        dhcp->options.opt[9] = 0x32;
        dhcp->options.opt[10] = 0x04;
        req = request->s_addr;
        dhcp->options.opt[11] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[12] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[13] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[14] = req & 0xFF;
        dhcp->options.opt[15] = 0xff;
        break;
      case 0x4: // 0100, only lease
        dhcp->options.opt[3] = 0x33;
        dhcp->options.opt[4] = 0x04;
        lea = lease;
        dhcp->options.opt[5] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[6] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[7] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[8] = lea & 0xFF;
        dhcp->options.opt[9] = 0xff;
        break;
      case 0x5: // 0101, lease and server
        dhcp->options.opt[3] = 0x36;
        dhcp->options.opt[4] = 0x04;
        serv = server->s_addr;
        dhcp->options.opt[5] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[6] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[7] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[8] = serv & 0xFF;
        dhcp->options.opt[9] = 0x33;
        dhcp->options.opt[10] = 0x04;
        lea = lease;
        dhcp->options.opt[11] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[12] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[13] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[14] = lea & 0xFF;
        dhcp->options.opt[15] = 0xff;
        break;
      case 0x6: // 0110, lease and request
        dhcp->options.opt[3] = 0x32;
        dhcp->options.opt[4] = 0x04;
        req = request->s_addr;
        dhcp->options.opt[5] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[6] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[7] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[8] = req & 0xFF;
        dhcp->options.opt[9] = 0x33;
        dhcp->options.opt[10] = 0x04;
        lea = lease;
        dhcp->options.opt[11] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[12] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[13] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[14] = lea & 0xFF;
        dhcp->options.opt[15] = 0xff;
        break;
      case 0x7: // 0111, all three
        dhcp->options.opt[3] = 0x36;
        dhcp->options.opt[4] = 0x04;
        serv = server->s_addr;
        dhcp->options.opt[5] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[6] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[7] = serv & 0xFF;
        serv = serv >> 8;
        dhcp->options.opt[8] = serv & 0xFF;
        dhcp->options.opt[9] = 0x32;
        dhcp->options.opt[10] = 0x04;
        req = request->s_addr;
        dhcp->options.opt[11] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[12] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[13] = req & 0xFF;
        req = req >> 8;
        dhcp->options.opt[14] = req & 0xFF;
        dhcp->options.opt[15] = 0x33;
        dhcp->options.opt[16] = 0x04;
        lea = lease;
        dhcp->options.opt[17] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[18] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[19] = lea & 0xFF;
        lea = lea >> 8;
        dhcp->options.opt[20] = lea & 0xFF;
        dhcp->options.opt[21] = 0xff;
        break;
      default:
        exit (1);
        break;
    }
}

int
calculate_extra_bytes (dhcp_msg_t *dhcp)
{
  int extra_bytes = 5; // accounting for cookie size of 4 bytes and final byte 0xff
  int i = 0;
  while (dhcp->options.opt[i] != 0xff)
    {
      extra_bytes++;
      i++;
    }
  return extra_bytes++;
}
