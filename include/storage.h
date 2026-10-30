#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

int initStorage(const char *data_dir);

/* Section 9 Required Functions */
int saveData(void);
int loadData(void);

#endif /* STORAGE_H */
