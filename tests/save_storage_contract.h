#ifndef TESTS_SAVE_STORAGE_CONTRACT_H
#define TESTS_SAVE_STORAGE_CONTRACT_H

/* The behavior every save_storage backend shares, through the three
 * functions only. Starts with an erase and leaves the storage empty. */
void save_storage_contract_run(void);

#endif
