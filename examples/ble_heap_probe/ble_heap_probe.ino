#include <Arduino.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp_heap_caps.h>
extern "C" {
#include "host/ble_gap.h"
#include "host/ble_hs.h"
}
static void *blocks[1024];
static size_t count;
static volatile size_t failSize;
static volatile uint32_t failCaps;
static volatile unsigned failures;
static void failed(size_t size, uint32_t caps, const char *) {
  failSize = size; failCaps = caps; ++failures;
}
static void heap(const char *phase) {
  const uint32_t caps[] = {MALLOC_CAP_INTERNAL, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,
    MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT|MALLOC_CAP_DMA};
  Serial.printf("HEAP %s", phase);
  for (unsigned i=0;i<3;i++) Serial.printf(" cap%u_free=%u cap%u_largest=%u",i,
    (unsigned)heap_caps_get_free_size(caps[i]),i,(unsigned)heap_caps_get_largest_free_block(caps[i]));
  Serial.printf(" failures=%u last_size=%u last_caps=0x%lx\n",failures,(unsigned)failSize,(unsigned long)failCaps);
}
static int event(struct ble_gap_event *, void *) { return 0; }
void setup() {
  Serial.begin(115200); delay(1500);
  Serial.printf("PROBE core=%s idf=%s psram=%u\n",ESP_ARDUINO_VERSION_STR,esp_get_idf_version(),(unsigned)ESP.getPsramSize());
  heap_caps_register_failed_alloc_callback(failed);
  WiFi.mode(WIFI_OFF); heap("before-init");
  BLEDevice::init("c5-heap-probe");
  auto server=BLEDevice::createServer();
  auto svc=server->createService("12345678-1234-1234-1234-123456789abc");
  svc->createCharacteristic("12345678-1234-1234-1234-123456789abd",BLECharacteristic::PROPERTY_READ);
  svc->start(); server->start(); heap("after-gatt");
  const uint8_t data[]={2,1,6,17,7,0xbc,0x9a,0x78,0x56,0x34,0x12,0x34,0x12,0x34,0x12,0x34,0x12,0x78,0x56,0x34,0x12};
  int drc=ble_gap_adv_set_data(data,sizeof(data));
  int src=ble_gap_adv_rsp_set_data(nullptr,0);
  Serial.printf("CONFIG data_rc=%d scan_rc=%d\n",drc,src);
  Serial.println("Commands: p <target-free-bytes> (pressure 256-byte blocks), f (free alternating blocks), a (start), s (stop), r (release all), x (reboot), h (heap)");
}
void loop() {
  if (!Serial.available()) {delay(10);return;}
  char c=Serial.read();
  if(c=='p') {
    size_t target=Serial.parseInt();
    while(count<1024 && heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>target+272) {
      void *p=heap_caps_malloc(256,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
      if(!p)break; blocks[count++]=p;
    }
    heap("pressure");
  }
  if(c=='f') {for(size_t i=0;i<count;i+=2){heap_caps_free(blocks[i]);blocks[i]=nullptr;}heap("fragmented");}
  if(c=='r'){for(size_t i=0;i<count;i++)heap_caps_free(blocks[i]);count=0;heap("released");}
  if(c=='h')heap("query");
  if(c=='x'){ESP.restart();}
  if(c=='s'){Serial.printf("STOP rc=%d\n",ble_gap_adv_stop());heap("stopped");}
  if(c=='a') {
    uint8_t addr; int rc=ble_hs_id_infer_auto(0,&addr);
    struct ble_gap_adv_params params={};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    params.itvl_min=160;params.itvl_max=240;
    heap("before-start");
    if(!rc)rc=ble_gap_adv_start(addr,nullptr,BLE_HS_FOREVER,&params,event,nullptr);
    Serial.printf("ADV rc=%d active=%d\n",rc,ble_gap_adv_active());
    heap("after-start");
  }
}
