/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "driver/gptimer.h"
#include "esp_log.h"

static const char *TAG = "example";
gptimer_handle_t gptimer = NULL;	//таймер для детектора нуля
#define gptimerDeadTime 100		//длина импульса (зависит от таймера)>20
typedef struct {
    uint64_t event_count;
} example_queue_element_t;


#define CONFIG_GPIO_OUTPUT_0 	18
#define CONFIG_GPIO_INPUT_0		4

#define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0))
#define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))

//static QueueHandle_t gpio_evt_queue = NULL;
bool EnGpTimer_DetectZero = false;// состояние детектирования нуля
int OffsetTimeFromZero=0;

static void IRAM_ATTR gpio_isr_handler(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args)//запуск таймера при пересечении нуля
{
		gptimer_start(gptimer);
}

static bool IRAM_ATTR  gptimer_on_alarm (gptimer_handle_t gptimer, const gptimer_alarm_event_data_t *edata, void *user_data)//по таймеру импульс на выход GPIO
{
	static bool DeadTime;
	if (DeadTime) {
		gpio_set_level(GPIO_OUTPUT_IO_0, true);
		// reconfigure alarm value
		gptimer_alarm_config_t alarm_config = {
           .alarm_count = edata->alarm_value + gptimerDeadTime, // alarm in next
			};
       gptimer_set_alarm_action(gptimer, &alarm_config);
       DeadTime=false;
	} else {
	    gpio_set_level(GPIO_OUTPUT_IO_0, false);
	    // reconfigure alarm value
	    gptimer_alarm_config_t alarm_config = {
	       .alarm_count = edata->alarm_value + OffsetTimeFromZero, // alarm in next
	       };
	    gptimer_set_alarm_action(gptimer, &alarm_config);
		gptimer_stop(gptimer);
		DeadTime=true;
	}
	BaseType_t high_task_awoken = pdFALSE;
	    QueueHandle_t queue = (QueueHandle_t)user_data;
    // Retrieve count value and send to queue
    example_queue_element_t ele = {
        .event_count = edata->count_value
    };
    xQueueSendFromISR(queue, &ele, &high_task_awoken);
       // return whether we need to yield at the end of ISR
    return (high_task_awoken == pdTRUE);
}

void app_main(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE ;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    //install gpio isr service ESP_INTR_FLAG_EDGE
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);

    example_queue_element_t ele; // для Gptimer из примера
        QueueHandle_t queue = xQueueCreate(10, sizeof(example_queue_element_t));

        if (!queue) {
            ESP_LOGE(TAG, "Creating queue failed");
            return;
        }

    ESP_LOGI(TAG, "Create timer handle");

    	//gptimer_handle_t gptimer = NULL;
    	gptimer_config_t timer_config = {
           .clk_src = GPTIMER_CLK_SRC_APB,
           .direction = GPTIMER_COUNT_UP,
           .resolution_hz = 1*1000*1000, // 1MHz, 1 tick=1us
       };
       ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

       gptimer_event_callbacks_t cbs = {
           .on_alarm = gptimer_on_alarm,
       };
       //cbs.on_alarm = gptimer_on_alarm;
       ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, queue));

       ESP_LOGI(TAG, "Start timer, auto-reload at alarm event");
       gptimer_alarm_config_t alarm_config1 = {
//   		   .reload_count = 6-5, //1-Импульс на выходе появляется через 6-8-16мкс <10*1000
			   .alarm_count = 0, //
//			   .flags.auto_reload_on_alarm = true,
       };
       ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config1));


       if (xQueueReceive(queue, &ele, pdMS_TO_TICKS(2000))) {
               ESP_LOGI(TAG, "Timer stopped, count=%llu", ele.event_count);
           } else {
               ESP_LOGW(TAG, "Missed one count event");
           }

    printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());

    uint64_t  cnt = 0;
    uint64_t  LastCnt = 0;
    bool EnDetectZero = false; //импульс включения детектора нуля
    bool DisDetectZero = false;//импульс выключения детектора нуля
    int Hz=0;// частота импульсов прерывания
    while(1) {
    	if (EnDetectZero||DisDetectZero) {// обработка импульса "включение или выключение" таймера и детектора нуля
    		if (EnDetectZero){//если детектируем нуль
    			if (gptimer_enable(gptimer) == ESP_OK){
    			//hook isr handler for specific gpio pin
    			gpio_isr_handler_add(GPIO_INPUT_IO_0,(void*) gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    			EnGpTimer_DetectZero = true;
    			EnDetectZero = false;
    			}
    		}else if (DisDetectZero) {
    			//remove isr handler for gpio number.
    			if (gpio_isr_handler_remove(GPIO_INPUT_IO_0) == ESP_OK){
    				if (gptimer_stop(gptimer)== ESP_OK){
    				gptimer_disable(gptimer);
    				EnGpTimer_DetectZero = false;
    				DisDetectZero=false;//обнуляем импульс
    				}
    			}
    		}
    	}

    	OffsetTimeFromZero = 20;

    	 //считаем частоту импульсов прерывания
    	ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &cnt));
    	Hz=(cnt-LastCnt)/(OffsetTimeFromZero+gptimerDeadTime);
    	printf("Hz: %d\n", Hz);
    	//printf("Hz: %lld\n", Hz);
        LastCnt = cnt;

        if (EnGpTimer_DetectZero) DisDetectZero = true; else  EnDetectZero = true;




        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }
}
