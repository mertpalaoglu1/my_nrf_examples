#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus_client.h>
#include <bluetooth/scan.h>
#include <bluetooth/gatt_dm.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>




LOG_MODULE_REGISTER(central_uart);

static struct bt_conn *default_conn;
static struct bt_nus_client nus_client;
static struct k_work scan_work;
int ble_status = 0;

#define DEV_CONNECTED     1
#define DEV_DISCONNECTED  0
static struct bt_uuid_128 nus_uuid = BT_UUID_INIT_128(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

// === BLE callback'ları ===

static void ble_data_sent(struct bt_nus_client *nus, uint8_t err,
                          const uint8_t *const data, uint16_t len) {
    ARG_UNUSED(nus);
    ARG_UNUSED(data);
    ARG_UNUSED(len);
    if (err) {
        LOG_WRN("ATT error code: 0x%02X", err);
    }
}

static uint8_t ble_data_received(struct bt_nus_client *nus,
                                 const uint8_t *data, uint16_t len) {
    ARG_UNUSED(nus);
    ARG_UNUSED(data);
    ARG_UNUSED(len);
    // Veri almak bu örnekte kullanılmıyor.
    return BT_GATT_ITER_CONTINUE;
}

// === PHY Güncelleme ===

static void update_phy(struct bt_conn *conn) {
    int err;
    struct bt_conn_le_phy_param phy = {
        .options = BT_CONN_LE_OPT_NONE,
        .pref_rx_phy = BT_GAP_LE_PHY_CODED,
        .pref_tx_phy = BT_GAP_LE_PHY_CODED,
    };

    err = bt_conn_le_phy_update(conn, &phy);
    if (err) {
        LOG_ERR("PHY update failed (err %d)", err);
    } else {
        LOG_INF("PHY update requested: Coded PHY");
    }
}

// === GATT discovery ===

static void discovery_complete(struct bt_gatt_dm *dm, void *context) {
    struct bt_nus_client *nus = context;

    bt_nus_handles_assign(dm, nus);
    bt_nus_subscribe_receive(nus);
    bt_gatt_dm_data_release(dm);

    LOG_INF("Service discovery complete");
}

static void discovery_error(struct bt_conn *conn, int err, void *context) {
    LOG_ERR("GATT discovery error (%d)", err);
}

static void discovery_service_not_found(struct bt_conn *conn, void *context) {
    LOG_WRN("Service not found during discovery");
}

static struct bt_gatt_dm_cb discovery_cb = {
    .completed = discovery_complete,
    .service_not_found = discovery_service_not_found,
    .error_found = discovery_error,
};

// === Bağlantı olayları ===

static void connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    int tx_err = bt_conn_le_set_tx_power(conn, 8);
    if (tx_err) {
        LOG_ERR("Failed to set TX power (err %d)", tx_err);
    } else {
        LOG_INF("TX power set to +8 dBm");
    }   


    if (err) {
        LOG_ERR("Connection failed to %s (%u)", addr, err);
        return;
    }

    default_conn = bt_conn_ref(conn);
    ble_status = DEV_CONNECTED;
    LOG_INF("Connected: %s", addr);

    update_phy(conn);

    int discover_err = bt_gatt_dm_start(conn,&nus_uuid.uuid, &discovery_cb, &nus_client);
    if (discover_err) {
        LOG_ERR("GATT Discovery start failed (err %d)", discover_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected from %s (reason 0x%02x)", addr, reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }

    ble_status = DEV_DISCONNECTED;
    k_work_submit(&scan_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// === Scan olayları ===

static void scan_connecting(struct bt_scan_device_info *device_info, struct bt_conn *conn) {
    default_conn = bt_conn_ref(conn);
}

static void scan_connecting_error(struct bt_scan_device_info *device_info) {
    LOG_WRN("Connection attempt failed");
}

static void scan_filter_match(struct bt_scan_device_info *device_info,
                              struct bt_scan_filter_match *filter_match,
                              bool connectable) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
    LOG_INF("Filter matched, connecting to %s", addr);
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL,
                scan_connecting_error, scan_connecting);

static void scan_work_handler(struct k_work *item) {
    ARG_UNUSED(item);
    bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
}

static void scan_init(void) {
    struct bt_scan_init_param scan_init = {
        .connect_if_match = true,
    };
   bt_scan_init(&scan_init);
    bt_scan_cb_register(&scan_cb);

    // UUID filtresi kaldırılabilir veya bırakılabilir
    // bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID,  &nus_uuid.uuid);
    // bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);

    // İsim filtresi ekle
    bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, "Nordic_UART_Service");
    bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false);

    k_work_init(&scan_work, scan_work_handler);
}

// === NUS client init ===

static int nus_client_init(void) {
    struct bt_nus_client_init_param init = {
        .cb = {
            .received = ble_data_received,
            .sent = ble_data_sent,
        }
    };

    return bt_nus_client_init(&nus_client, &init);
}

// === Main ===

int main(void) {
    int err;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }
    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = nus_client_init();
    if (err) {
        LOG_ERR("NUS client init failed (err %d)", err);
        return;
    }

    scan_init();
    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Scan start failed (err %d)", err);
        return;
    }

    LOG_INF("BLE Long Range test app started");

    // Döngü: Bağlıysa her 500ms'de bir veri gönder
    while (1) {
        if (ble_status == DEV_CONNECTED) {
            char msg[32];
            memset(msg, 'A', sizeof(msg));
            err = bt_nus_client_send(&nus_client, msg, sizeof(msg));
            if (err) {
                LOG_WRN("BLE send failed: %d", err);
            } else {
                LOG_INF("Data sent");
            }
        }

        k_sleep(K_MSEC(500));
    }
}
