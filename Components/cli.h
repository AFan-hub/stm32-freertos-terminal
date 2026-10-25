#ifndef CLI_H
#define CLI_H

/* Serial command line on USART1. Lines are assembled by CommTask from the
 * RX ring and handed here; all output goes through the async UART path.
 */
void CLI_ProcessLine(char *line);

#endif
