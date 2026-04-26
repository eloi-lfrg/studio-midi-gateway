# Composant `networking`

Couche de communication sans fil basée sur **ESP-NOW** pour ESP32-S3 / ESP-IDF v6.0.

Le composant fournit deux rôles distincts : **Node** (émetteur) et **Gateway** (récepteur), plus les types de frames partagés.

---

## Architecture

```
Node                          Gateway
────                          ───────
begin()                       begin()
  └─ esp_now_init               └─ esp_now_init
  └─ add_peer(gateway MAC)      └─ register recv CB
  └─ register send CB           └─ start processTask
connect(key, cb)
  └─ register recv CB
  └─ send [CONNECT, key]
                           ──►  handleConnect()
                                  └─ valide la clé
                                  └─ add_peer(node MAC)
                                  └─ send [CONNACK, status]
  s_recvCb()              ◄──
    └─ cb(accepted)

send(payload, len)         ──►  s_recvCb → queue → processTask → onFrame(frame)
```

---

## `Frame.hpp`

Types et constantes partagés entre Node et Gateway.

```cpp
// Taille maximale du payload
static constexpr size_t FRAME_PAYLOAD_MAX_SIZE = 250;

// Types de frames (premier octet du payload)
static constexpr uint8_t FRAME_TYPE_CONNECT = 0xC0;
static constexpr uint8_t FRAME_TYPE_CONNACK = 0xC1;

// Codes CONNACK (convention MQTT/BLE : 0x00 = accepté)
static constexpr uint8_t CONNACK_ACCEPTED = 0x00;
static constexpr uint8_t CONNACK_REFUSED  = 0x01;

// Longueur fixe de la clé d'authentification
static constexpr size_t CONNECT_KEY_LEN = 4;

struct Frame {
    std::array<uint8_t, 6> srcMac;      // MAC source
    uint8_t payload[FRAME_PAYLOAD_MAX_SIZE];
    size_t  payloadLen;
};
```

### Format des frames

| Type | Direction | Payload |
|------|-----------|---------|
| `CONNECT` (`0xC0`) | Node → Gateway | `[0xC0, key[0], key[1], key[2], key[3]]` |
| `CONNACK` (`0xC1`) | Gateway → Node | `[0xC1, 0x00]` accepté \| `[0xC1, 0x01]` refusé |
| Données applicatives | Node → Gateway | contenu libre (ex. `[0xB0, cc, value]` pour MIDI) |

---

## `Node`

Nœud émetteur. S'authentifie auprès de la gateway puis envoie des frames.

### API publique

```cpp
// Constructeur — MAC de la gateway cible
explicit Node(const std::array<uint8_t, 6> &gatewayMac);

// Initialise ESP-NOW et enregistre la gateway comme pair
esp_err_t begin();

// Envoie une frame CONNECT et installe le callback CONNACK
// À appeler après begin()
esp_err_t connect(const std::array<uint8_t, CONNECT_KEY_LEN> &key, OnConnectCb cb);

// Envoie un payload arbitraire vers la gateway
// Bloquant jusqu'à l'ACK RF (timeout 50 ms) — sérialisé par sémaphore
esp_err_t send(const uint8_t *data, size_t len);
```

### Callback

```cpp
using OnConnectCb = std::function<void(bool accepted)>;
```

Appelé depuis le contexte du callback ESP-NOW recv (WiFi task) à la réception du CONNACK.

### Gestion de la mémoire ESP-NOW

`send()` utilise un sémaphore binaire libéré par le send callback RF. Cela sérialise les envois et empêche le débordement de la queue interne ESP-NOW (10 slots).

```
send() → take(sem, 50ms) → esp_now_send() → [RF] → s_sendCb() → give(sem)
```

Si le sémaphore n'est pas disponible dans les 50 ms, `send()` retourne `ESP_ERR_TIMEOUT`.

---

## `Gateway`

Nœud récepteur. Accepte les connexions des nœuds authentifiés et distribue les frames.

### API publique

```cpp
// Constructeur — profondeur de la queue de réception (défaut : 16 frames)
explicit Gateway(size_t queueDepth = 16);

// Définit la clé partagée — doit être appelé avant begin()
void setConnectKey(const std::array<uint8_t, CONNECT_KEY_LEN> &key);

// Initialise ESP-NOW, démarre la tâche de traitement
esp_err_t begin();

// Enregistre le callback appelé pour chaque frame non-CONNECT reçue
void setOnFrameCb(OnFrameCb cb);
```

### Callback

```cpp
using OnFrameCb = std::function<void(const Frame &)>;
```

Appelé depuis la tâche `networking_rx` (pas depuis une ISR).

### Traitement des frames CONNECT

Lorsqu'une frame `CONNECT` est reçue :

1. La clé reçue est comparée à la clé configurée via `setConnectKey()`.
2. Le pair (MAC de la node) est enregistré dynamiquement via `esp_now_add_peer()` s'il n'existe pas encore.
3. Un CONNACK est renvoyé (`0x00` si clé valide, `0x01` sinon).
4. La frame n'est **pas** transmise au callback `onFrame`.

Si `setConnectKey()` n'a pas été appelé, toutes les connexions sont refusées.

### Architecture interne

```
s_recvCb() [IRAM, WiFi task]
    └─ xQueueSendFromISR()
           │
           ▼
    processTask() [FreeRTOS task "networking_rx"]
        ├─ CONNECT  → handleConnect()
        └─ autres   → onFrame(frame)
```

---

## Exemples

### Gateway — `main.cpp` minimal

```cpp
#include "Gateway.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static constexpr const char *TAG = "main";

static constexpr std::array<uint8_t, networking::CONNECT_KEY_LEN> KEY = {0x4B, 0x4E, 0x4F, 0x42};

static networking::Gateway s_gateway;

static void wifiInit() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
}

extern "C" void app_main() {
    wifiInit();

    s_gateway.setConnectKey(KEY);
    s_gateway.setOnFrameCb([](const networking::Frame &frame) {
        ESP_LOGI(TAG, "frame from %02X:%02X:%02X:%02X:%02X:%02X len=%zu",
                 frame.srcMac[0], frame.srcMac[1], frame.srcMac[2],
                 frame.srcMac[3], frame.srcMac[4], frame.srcMac[5],
                 frame.payloadLen);
    });

    ESP_ERROR_CHECK(s_gateway.begin());
}
```

### Node — `main.cpp` minimal

```cpp
#include "Node.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static constexpr const char *TAG = "main";

static constexpr std::array<uint8_t, networking::CONNECT_KEY_LEN> KEY = {0x4B, 0x4E, 0x4F, 0x42};

static networking::Node s_node({0xAC, 0xA7, 0x04, 0x2C, 0x7F, 0x24});

static void wifiInit() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
}

extern "C" void app_main() {
    wifiInit();
    ESP_ERROR_CHECK(s_node.begin());

    ESP_ERROR_CHECK(s_node.connect(KEY, [](bool accepted) {
        if (accepted) {
            ESP_LOGI(TAG, "connected to gateway");
        } else {
            ESP_LOGE(TAG, "connection refused");
        }
    }));

    // Envoi périodique d'un payload applicatif
    uint8_t counter = 0;
    while (true) {
        uint8_t payload[3] = {0xB0, 0x01, counter++};
        esp_err_t ret = s_node.send(payload, sizeof(payload));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "send failed: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

---

## Dépendances CMake

```cmake
idf_component_register(
    SRCS "Gateway.cpp" "Node.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
)
```

Le composant requiert que le WiFi soit initialisé et démarré **avant** tout appel à `begin()`.
