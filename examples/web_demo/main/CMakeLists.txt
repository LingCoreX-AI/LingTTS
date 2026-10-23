idf_component_register(SRCS "main.c"
    PRIV_REQUIRES lingtts esp_driver_i2s esp_timer heap
                  esp_wifi esp_netif esp_event nvs_flash esp_http_server json
    EMBED_TXTFILES "index.html")
