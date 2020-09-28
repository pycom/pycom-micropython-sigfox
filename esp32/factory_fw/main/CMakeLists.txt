idf_component_register(SRCS "factory_fw_main.c" "pycom_config.c" "wifi_conn.c"
                    INCLUDE_DIRS "."
                    # Embed the server root certificate into the final binary
                    EMBED_TXTFILES ${project_dir}/server_certs/pycom_ca_cert.pem)
