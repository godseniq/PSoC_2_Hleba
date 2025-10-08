#include "project.h" 

static void FourDigit74HC595_sendData(uint8_t data);
static void FourDigit74HC595_sendOneDigit(uint8_t position, uint8_t digit, uint8_t dot);
static void initMatrix(void);
static void readMatrix(void);

static uint8_t LED_NUM[] = { 
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 
    0xBF, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E
}; 

static uint8_t led_counter = 0;
static uint8_t display_data[8] = {7, 6, 5, 4, 3, 2, 1, 0}; // Як в завданні

CY_ISR(Timer_Int_Handler2)
{
    FourDigit74HC595_sendOneDigit(led_counter, display_data[led_counter], 0);
    led_counter++;
    if(led_counter > 7) led_counter = 0;
}

static void FourDigit74HC595_sendData(uint8_t data) {     
    for(uint8_t i = 0; i < 8; i++) {         
        if(data & (0x80 >> i)) { 
            Pin_DO_Write(1); 
        } else { 
            Pin_DO_Write(0); 
        } 
        Pin_CLK_Write(1); 
        Pin_CLK_Write(0); 
    } 
} 

static void FourDigit74HC595_sendOneDigit(uint8_t position, uint8_t digit, uint8_t dot)  
{ 
    if(position >= 8) { 
        FourDigit74HC595_sendData(0xFF); 
        FourDigit74HC595_sendData(0xFF); 
        return;
    } 
    FourDigit74HC595_sendData(0xFF & ~(1 << position));     
    if(dot) { 
        FourDigit74HC595_sendData(LED_NUM[digit] & 0x7F); 
    } else { 
        FourDigit74HC595_sendData(LED_NUM[digit]); 
    } 
    Pin_Latch_Write(1);
    Pin_Latch_Write(0); 
} 

static uint8_t keys[4][3] = { 
    {1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 0, 11}
}; 

static void (*COLUMN_x_SetDriveMode[3])(uint8_t mode) = { 
    COLUMN_0_SetDriveMode, COLUMN_1_SetDriveMode, COLUMN_2_SetDriveMode 
};   

static void (*COLUMN_x_Write[3])(uint8_t value) = { 
    COLUMN_0_Write, COLUMN_1_Write, COLUMN_2_Write 
}; 

static uint8 (*ROW_x_Read[4])() = { 
    ROW_0_Read, ROW_1_Read, ROW_2_Read, ROW_3_Read 
}; 

static void initMatrix(void) 
{     
    for(int i = 0; i < 3; i++) { 
        COLUMN_x_SetDriveMode[i](COLUMN_0_DM_DIG_HIZ); 
    } 
} 

static void readMatrix(void) 
{ 
    uint8_t row_counter = sizeof(ROW_x_Read)/sizeof(ROW_x_Read[0]); 
    uint8_t column_counter = sizeof(COLUMN_x_Write)/sizeof(COLUMN_x_Write[0]);     
    
    for(int column_index = 0; column_index < column_counter; column_index++) { 
        COLUMN_x_SetDriveMode[column_index](COLUMN_0_DM_STRONG); 
        COLUMN_x_Write[column_index](0);         
        
        for(int row_index = 0; row_index < row_counter; row_index++) { 
            keys[row_index][column_index] = ROW_x_Read[row_index](); 
        } 
        
        COLUMN_x_SetDriveMode[column_index](COLUMN_0_DM_DIG_HIZ); 
    } 
} 

// Функція для циклічного зсуву вправо останніх 4 символів
void shiftRightLast4(void) 
{
    // Зберігаємо останній елемент (позиція 7)
    uint8_t temp = display_data[7];
    
    // Зсуваємо останні 4 символи (позиції 4-7) вправо
    for(int i = 7; i > 4; i--) {
        display_data[i] = display_data[i-1];
    }
    
    // Перший з останніх 4 отримує збережене значення
    display_data[4] = temp;
}

int main(void) 
{ 
    CyGlobalIntEnable;
    
    Timer_Start();
    Timer_Int_StartEx(Timer_Int_Handler2);
    
    initMatrix(); 
    uint8_t last_state = 12; 
    uint32_t shift_timer = 0;
    
    for(;;) 
    { 
        readMatrix(); 
        
        if(keys[0][0] == 0 && last_state != 1) { 
            last_state = 1; 
            display_data[0] = 1;
        } 
        if(keys[0][0] == 1 && last_state == 1) last_state = 12;
        
        if(keys[0][1] == 0 && last_state != 2) { 
            last_state = 2; 
            display_data[0] = 2;
        } 
        if(keys[0][1] == 1 && last_state == 2) last_state = 12;
        
        if(keys[0][2] == 0 && last_state != 3) { 
            last_state = 3; 
            display_data[0] = 3;
        } 
        if(keys[0][2] == 1 && last_state == 3) last_state = 12;
        
        if(keys[1][0] == 0 && last_state != 4) { 
            last_state = 4; 
            display_data[0] = 4;
        } 
        if(keys[1][0] == 1 && last_state == 4) last_state = 12;
        
        if(keys[1][1] == 0 && last_state != 5) { 
            last_state = 5; 
            display_data[0] = 5;
        } 
        if(keys[1][1] == 1 && last_state == 5) last_state = 12;
        
        if(keys[1][2] == 0 && last_state != 6) { 
            last_state = 6; 
            display_data[0] = 6;
        } 
        if(keys[1][2] == 1 && last_state == 6) last_state = 12;
        
        if(keys[2][0] == 0 && last_state != 7) { 
            last_state = 7; 
            display_data[0] = 7;
        } 
        if(keys[2][0] == 1 && last_state == 7) last_state = 12;
        
        if(keys[2][1] == 0 && last_state != 8) { 
            last_state = 8; 
            display_data[0] = 8;
        } 
        if(keys[2][1] == 1 && last_state == 8) last_state = 12;
        
        if(keys[2][2] == 0 && last_state != 9) { 
            last_state = 9; 
            display_data[0] = 9;
        } 
        if(keys[2][2] == 1 && last_state == 9) last_state = 12;
        
        if(keys[3][1] == 0 && last_state != 0) { 
            last_state = 0; 
            display_data[0] = 0;
        } 
        if(keys[3][1] == 1 && last_state == 0) last_state = 12;
        
        // Варіант 5: Перші чотири фіксовані, наступні вправо
        shift_timer++;
        if(shift_timer >= 50) { // Зсув кожні ~500мс
            shift_timer = 0;
            shiftRightLast4();
        }
        
        CyDelay(10);
    } 
}