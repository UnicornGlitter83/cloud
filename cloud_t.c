// cc -o cloud_t cloud_t.c -lmicrohttpd -ljansson

#include <microhttpd.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define PORT 8888
#define BUFFER_SIZE 8192
#define MAX_ITEMS 100

typedef struct {
	char id[64];
	char *json;
} Item;

typedef struct {
	char *post_data;
	size_t post_data_size;
} connection_info_struct;

struct connection_info_struct {
	char *post_data;
	size_t post_data_size;
};
//memory store sim

Item database[MAX_ITEMS];
int item_count = 0;
int id_counter = 1;

const char *gen_id() {
	static char id[16];
	snprintf(id, sizeof(id), "%d", id_counter++);
	return id;
}

enum MHD_Result send_response(struct MHD_Connection *connection, const char *msg, int status_code) {
	struct MHD_Response *response = MHD_create_response_from_buffer(strlen(msg), (void*) msg, MHD_RESPMEM_MUST_COPY);
	int ret = MHD_queue_response(connection, status_code, response);
	MHD_destroy_response(response);
	return ret;
}

static enum MHD_Result answer_to_connection(void *cls,
			   		    struct MHD_Connection *connection,
					    const char *url,
					    const char *method,
					    const char *version,
					    const char *upload_data,
					    size_t *upload_data_size,
					    void **con_cls)
{
	struct connection_info_struct *con_info = *con_cls;
	if (con_info == NULL) {
		con_info = malloc(sizeof(connection_info_struct));
		if (!con_info)
			return MHD_NO;
		con_info->post_data = NULL;
		con_info->post_data_size = 0;
		*con_cls = con_info;

		printf("Received %s request for URL : %s\n", method, url);
		fflush(stdout);

		return MHD_YES;
	}

	if (*upload_data_size != 0) {
		con_info->post_data = realloc(con_info->post_data, con_info->post_data_size + *upload_data_size + 1);
		if (!con_info->post_data)
			return MHD_NO;
		memcpy(con_info->post_data + con_info->post_data_size, upload_data, *upload_data_size);
		con_info->post_data_size += *upload_data_size;
		con_info->post_data[con_info->post_data_size] = '\0';

		*upload_data_size = 0;
		return MHD_YES;
	}


	if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
		json_error_t error;
		json_t *root = json_loads(con_info->post_data ? con_info->post_data : "{}", 0, &error);
		if (!root || !json_is_object(root)) {
			free(con_info->post_data);
			free(con_info);
			*con_cls = NULL;
			return send_response(connection,  "{\"error\":\"JSON Invalid\"}", 400);
		}



		if (item_count >= MAX_ITEMS) {
			json_decref(root);
			free(con_info->post_data);
			free(con_info);
			return send_response(connection, "{\"error\":\"Database full\"}", 500);
		}

		const char *id = gen_id();
		strncpy(database[item_count].id, id, sizeof(database[item_count].id));
		database[item_count].id[sizeof(database[item_count].id) -1] = '\0';
		database[item_count].json = strdup(con_info->post_data);
		item_count++;

		char response[128];
		snprintf(response, sizeof(response), "{\"id\":\"%s\"}", id);
		json_decref(root);

		free(con_info->post_data);
		free(con_info);
		*con_cls = NULL;


		return send_response(connection, response, 201);
	}

	if (strcmp(method, "GET") == 0 || strcmp(method, "DELETE") == 0) {
		const char *req_id = url + 1;

		for (int i = 0; i < item_count; i++) {
			if (strcmp(database[i].id, req_id) == 0) {
				if (strcmp(method, "GET") == 0) {
					return send_response(connection, database[i].json, 200);
				} else {
					free(database[i].json);
					for(int j = i; j < item_count - 1; j++) {
						database[j] = database[j + 1];
					}
					item_count--;
					return send_response(connection, "{\"status\":\"deleted\"}", 200);
				}
			}
		}

		return send_response(connection, "{\"error\":\"Not found\"}", 404);
	}
	free(con_info->post_data);
	free(con_info);
	*con_cls = NULL;
	return send_response(connection, "{\"error\":\"Method Unsupported\"}", 405);
}


int main() {
	struct MHD_Daemon *daemon;

	daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_END);
	if (NULL == daemon) {
		fprintf(stderr, "HTTP server failed to start.\n");
		 return 1;
	}

	printf("Server running on http://localhost:%d/\n", PORT);
	while (1) {
		sleep(1);
	}

	MHD_stop_daemon(daemon);
	return 0;
}





