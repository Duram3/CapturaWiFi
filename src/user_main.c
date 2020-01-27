/* Envia datos provenientes del escaneo en canales WiFi al siguiente NodeMCU, a través del puerto serial
*/

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "stdbool.h"
#include "stdlib.h"
#include "stdio.h"


// ESP-12 modules have LED on GPIO2. Change to another GPIO
// for other boards.

os_timer_t TiempoEscaneo;
bool ini=true;

uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

//______________________________________________________
struct RxControl { 
    signed rssi:8;             // signal intensity of packet 
    unsigned rate:4; 
    unsigned is_group:1; 
    unsigned:1; 
    unsigned sig_mode:2;       // 0:is 11n packet; 1:is not 11n packet;  
    unsigned legacy_length:12; // if not 11n packet, shows length of packet.  
    unsigned damatch0:1; 
    unsigned damatch1:1; 
    unsigned bssidmatch0:1; 
    unsigned bssidmatch1:1; 
    unsigned MCS:7;            // if is 11n packet, shows the modulation  
                               // and code used (range from 0 to 76) 
    unsigned CWB:1; // if is 11n packet, shows if is HT40 packet or not  
    unsigned HT_length:16;// if is 11n packet, shows length of packet. 
    unsigned Smoothing:1; 
    unsigned Not_Sounding:1; 
    unsigned:1; 
    unsigned Aggregation:1; 
    unsigned STBC:2; 
    unsigned FEC_CODING:1; // if is 11n packet, shows if is LDPC packet or not. 
        unsigned SGI:1; 
    unsigned rxend_state:8; 
    unsigned ampdu_cnt:8; 
    unsigned channel:4; //which channel this packet in. 
    unsigned:12; 
};//RxControl Contiene diversa info sobre paquete capturado, fuente 1, copiado de página 136


typedef struct { 
    struct RxControl rx_ctrl; 
    u8 buf[112];  //esto se puede leer bit a bit para obtener dir MAC
    u16 cnt;    
    u16 len;  //length of packet 
} sniffer_buf; //en este buffer cae la información completa de paquete capturado, fuente 1, página 137


//__________________________________________________________
void VerificarCanales(){ //esto lo escribí para comprobar que podía elegir canales (1-13)

    os_printf("\nCanales disponibles:\n");

    for(int8 i=1; i<15; i++){

      if( wifi_set_channel(i) == true){          
          os_printf("\nCanal %d disponible:\n", i);
          }

      else{
          os_printf("\nCanal %d no disponible:\n", i);              
          }
    } 
};

//_______________________________________________________________


//________________________________________________________________
bool esProbeRequest(sniffer_buf *paquete){ 

    if ( (paquete->buf[0] & 48 >> 4) == 0 && ( paquete->buf[0] & 15 >> 1 ) == 4){
        return true;
    }
    else {return false;} 

}//esProbeRequest verifica si paquete entrante es un paquete de escaneo activo de un dispositivo

//________________________________________________________________
bool esManagement(sniffer_buf *paquete){//esManagement verifica si paquete entrante es un paquete de gestión (Management)


    if ( (paquete->buf[0] & 48 >> 4) == 0 ) {
        return true;
    }
    else {return false;} 

}

//_________________________________________________________________
void enviarDatos(sniffer_buf *paquete){

    uint8 msg[]={paquete->buf[10], paquete->buf[11], paquete->buf[12], paquete->buf[13], paquete->buf[14], paquete->buf[15], '\n'};
    uart0_tx_buffer(msg, 7);

}

//________________________________________________________________
void Captura(uint8_t *buffer, uint16_t len_buf){
    
    
    sniffer_buf *paquete = (sniffer_buf*) buffer;

    if ( esManagement(paquete)  == true ){
        enviarDatos(paquete);
    }

}


//________________________________________________________________
void CambiarCanal(){ 

    
    if(ini==true){

        wifi_promiscuous_enable(0);
        wifi_set_promiscuous_rx_cb(Captura); //cuando captura un paquete en modo promiscuo, llama a la función Captura, fuente 1, página 66
        wifi_promiscuous_enable(1);
        ini=false;
    }


    int8 n=wifi_get_channel();
    
    if(n+5 < 14){
        n+=5;
    }
    else{
        n-=9;
        if(n==0){n=1;}
    }

    wifi_set_channel(n);

    
};//CambiarCanal cambia cada 1 seg el canal escuchado, salta de 5 en 5 para manejar el solapamiento

//________________________________________________________________
void ICACHE_FLASH_ATTR user_init()
{

    uart_init(9600, 9600);
    wifi_set_opmode(0x01);    
    os_timer_disarm(&TiempoEscaneo);
    os_timer_setfn(&TiempoEscaneo, (os_timer_func_t *) CambiarCanal, NULL); //Cuando se cumplen 1000 ms, llama a la función CambiarCanal
    os_timer_arm(&TiempoEscaneo, 1000, true); //esto último es para activar un timer de software y cambiar canal cada cierto tiempo, fuente 1, página 14
   
}