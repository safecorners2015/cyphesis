// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000 Alistair Riddoch

#ifndef COMMON_LOG_H
#define COMMON_LOG_H

typedef enum log_level { INFO, NOTICE, WARNING, ERROR, CRITICAL } LogLevel;

void initLogger();
void log(LogLevel, const char * msg);

#endif // COMMON_LOG_H
