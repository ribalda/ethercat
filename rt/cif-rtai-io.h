unsigned long cif_open_card(
		unsigned int board_no,
		unsigned int in_size,
		unsigned int out_size,
		void (*callback)(unsigned long priv_data),
		unsigned long priv_data
		);

void cif_close_card(
		unsigned long fd
		);

int cif_reset_card(
		unsigned long fd,
		unsigned int timeout,
		unsigned int context	// 1 = interrupt context
		);

void cif_set_host_state(
		unsigned long fd,
		unsigned int state
		);

int cif_exchange_io(
		unsigned long fd,
		void *recv_data,
		void *send_data
		);

int cif_read_io(
		unsigned long fd,
		void *recv_data
		);

int cif_write_io(
		unsigned long fd,
		void *send_data
		);

int cif_card_ready(
		unsigned long fd
		);

