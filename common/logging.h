#ifndef SERPENT_LOGS_H
#define SERPENT_LOGS_H

void log_message(const char *message, const char *filename);
void log_server(const char *message);
void log_client(const char *message);

#endif //SERPENT_LOGS_H