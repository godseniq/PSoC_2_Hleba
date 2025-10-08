#include "project.h"

// Прототипи функцій
static void sendDataSPI(uint8_t data);
static void displayDigit(uint8_t position, uint8_t digit);
static void initMatrix(void);
static void readMatrix(void);
static void shiftRightLast4(void);
static void checkPassword(void);
static void updateTimer(void);
static void startTimer(void);
static void startPasswordInput(void);

// Коди для 7-сегментного індикатора (Common Anode)
// 0-9, A-F, всі увімкнені (8.), всі вимкнені ( )
static uint8_t SEGMENT_CODES[] = {
    0xC0, // 0
    0xF9, // 1
    0xA4, // 2
    0xB0, // 3
    0x99, // 4
    0x92, // 5
    0x82, // 6
    0xF8, // 7
    0x80, // 8
    0x90, // 9
    0x88, // A
    0x83, // B
    0xC6, // C
    0xA1, // D
    0x86, // E
    0x8E, // F
    0x00, // Всі увімкнені (8.)
    0xFF  // Всі вимкнені ( )
};

// Дані для відображення на 8 індикаторах
static uint8_t display_data[8] = {0, 0, 0, 0, 5, 4, 0, 0}; // Число 54

// Змінні для динамічної індикації
static uint8_t current_digit = 0;

// Змінні для паролю
static uint8_t password[4] = {1, 2, 3, 4}; // Пароль: 1234
static uint8_t input_buffer[4] = {0};
static uint8_t input_index = 0;
static uint8_t password_mode = 0;
static uint8_t password_verified = 0;

// Змінні для таймера
static uint8_t timer_minutes = 5;
static uint8_t timer_seconds = 0;
static uint32_t timer_counter = 0;
static uint8_t timer_running = 0;

// Змінні для анімації
static uint32_t shift_counter = 0;
static uint32_t blink_counter = 0;
static uint8_t blink_state = 0;

// Матриця клавіатури
static uint8_t keys[4][3] = {
    {1, 2, 3},
    {4, 5, 6}, 
    {7, 8, 9},
    {10, 0, 11} // * = 10, 0, # = 11
};

// Функції для роботи з стовпцями
static void (*COLUMN_SetDriveMode[3])(uint8_t mode) = {
    COLUMN_0_SetDriveMode,
    COLUMN_1_SetDriveMode, 
    COLUMN_2_SetDriveMode
};

static void (*COLUMN_Write[3])(uint8_t value) = {
    COLUMN_0_Write,
    COLUMN_1_Write,
    COLUMN_2_Write
};

// Функції для читання рядків
static uint8_t (*ROW_Read[4])() = {
    ROW_0_Read,
    ROW_1_Read,
    ROW_2_Read, 
    ROW_3_Read
};

// Обробник переривання таймера для динамічної індикації
CY_ISR(Timer_Display_Handler)
{
    if (password_verified == 0) {
        // Звичайний режим
        displayDigit(current_digit, display_data[current_digit]);
    }
    else if (password_verified == 1) {
        // Вірний пароль - мигання всіх сегментів
        displayDigit(current_digit, blink_state ? 16 : 17);
    }
    else if (password_verified == 2) {
        // Невірний пароль - шахове мигання
        if ((current_digit % 2 == 0 && blink_state) || (current_digit % 2 == 1 && !blink_state)) {
            displayDigit(current_digit, 16);
        } else {
            displayDigit(current_digit, 17);
        }
    }
    
    current_digit++;
    if (current_digit >= 8) current_digit = 0;
}

// Функція передачі даних по SPI
static void sendDataSPI(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (data & (0x80 >> i)) {
            Pin_DO_Write(1);
        } else {
            Pin_DO_Write(0);
        }
        Pin_CLK_Write(1);
        CyDelayUs(1);
        Pin_CLK_Write(0);
        CyDelayUs(1);
    }
}

// Функція відображення одного розряду
static void displayDigit(uint8_t position, uint8_t digit)
{
    // Вибір позиції (анод)
    uint8_t position_mask = 0xFF & ~(1 << position);
    sendDataSPI(position_mask);
    
    // Вибір сегментів (катод)
    if (digit < 18) {
        sendDataSPI(SEGMENT_CODES[digit]);
    } else {
        sendDataSPI(0xFF); // Вимкнено
    }
    
    // Зафіксувати дані
    Pin_Latch_Write(1);
    CyDelayUs(1);
    Pin_Latch_Write(0);
    CyDelayUs(1);
}

// Ініціалізація матриці клавіш
static void initMatrix(void)
{
    for (int i = 0; i < 3; i++) {
        COLUMN_SetDriveMode[i](COLUMN_0_DM_DIG_HIZ);
    }
}

// Читання стану матриці клавіш
static void readMatrix(void)
{
    for (int col = 0; col < 3; col++) {
        // Активувати стовпець
        COLUMN_SetDriveMode[col](COLUMN_0_DM_STRONG);
        COLUMN_Write[col](0);
        CyDelayUs(10);
        
        // Прочитати рядки
        for (int row = 0; row < 4; row++) {
            keys[row][col] = ROW_Read[row]();
        }
        
        // Деактивувати стовпець
        COLUMN_SetDriveMode[col](COLUMN_0_DM_DIG_HIZ);
    }
}

// Циклічний зсув останніх 4 цифр вправо (варіант 5)
static void shiftRightLast4(void)
{
    if (password_mode || password_verified || timer_running) return;
    
    uint8_t temp = display_data[7];
    for (int i = 7; i > 4; i--) {
        display_data[i] = display_data[i-1];
    }
    display_data[4] = temp;
}

// Перевірка паролю
static void checkPassword(void)
{
    password_verified = 1; // Припускаємо вірний пароль
    
    for (int i = 0; i < 4; i++) {
        if (input_buffer[i] != password[i]) {
            password_verified = 2; // Невірний пароль
            break;
        }
    }
    
    // Очистити буфер
    for (int i = 0; i < 4; i++) {
        input_buffer[i] = 0;
    }
    input_index = 0;
    password_mode = 0;
}

// Оновлення таймера
static void updateTimer(void)
{
    if (!timer_running) return;
    
    if (timer_seconds == 0) {
        if (timer_minutes == 0) {
            timer_running = 0;
            return;
        }
        timer_minutes--;
        timer_seconds = 59;
    } else {
        timer_seconds--;
    }
    
    // Оновити відображення
    display_data[0] = timer_minutes / 10;
    display_data[1] = timer_minutes % 10;
    display_data[2] = timer_seconds / 10;
    display_data[3] = timer_seconds % 10;
}

// Запуск таймера
static void startTimer(void)
{
    timer_running = 1;
    timer_minutes = 5;
    timer_seconds = 0;
    updateTimer();
}

// Початок вводу паролю
static void startPasswordInput(void)
{
    password_mode = 1;
    password_verified = 0;
    
    // Очистити буфер
    for (int i = 0; i < 4; i++) {
        input_buffer[i] = 0;
    }
    input_index = 0;
    
    // Відобразити індикацію вводу паролю
    for (int i = 0; i < 4; i++) {
        display_data[i] = 15; // 'F'
    }
    for (int i = 4; i < 8; i++) {
        display_data[i] = 17; // Порожньо
    }
}

int main(void)
{
    CyGlobalIntEnable;
    
    // Ініціалізація таймера для динамічної індикації
    Timer_Start();
    Timer_Int_StartEx(Timer_Display_Handler);
    
    initMatrix();
    
    uint8_t last_key_state = 12;
    uint32_t main_counter = 0;
    
    for (;;) {
        readMatrix();
        
        // Обробка натискань клавіш
        if (!password_mode && !password_verified) {
            // Звичайний режим
            if (keys[0][0] == 0 && last_key_state != 1) {
                last_key_state = 1;
                display_data[0] = 1;
            }
            if (keys[0][1] == 0 && last_key_state != 2) {
                last_key_state = 2;
                display_data[0] = 2;
            }
            if (keys[0][2] == 0 && last_key_state != 3) {
                last_key_state = 3;
                display_data[0] = 3;
            }
            if (keys[1][0] == 0 && last_key_state != 4) {
                last_key_state = 4;
                display_data[0] = 4;
            }
            if (keys[1][1] == 0 && last_key_state != 5) {
                last_key_state = 5;
                display_data[0] = 5;
            }
            if (keys[1][2] == 0 && last_key_state != 6) {
                last_key_state = 6;
                display_data[0] = 6;
            }
            if (keys[2][0] == 0 && last_key_state != 7) {
                last_key_state = 7;
                display_data[0] = 7;
            }
            if (keys[2][1] == 0 && last_key_state != 8) {
                last_key_state = 8;
                display_data[0] = 8;
            }
            if (keys[2][2] == 0 && last_key_state != 9) {
                last_key_state = 9;
                display_data[0] = 9;
            }
            if (keys[3][1] == 0 && last_key_state != 0) {
                last_key_state = 0;
                display_data[0] = 0;
            }
            
            // Кнопка * - запуск таймера
            if (keys[3][0] == 0 && last_key_state != 10) {
                last_key_state = 10;
                startTimer();
            }
            
            // Кнопка # - ввід паролю
            if (keys[3][2] == 0 && last_key_state != 11) {
                last_key_state = 11;
                startPasswordInput();
            }
        }
        else if (password_mode) {
            // Режим вводу паролю
            if (keys[0][0] == 0 && last_key_state != 1 && input_index < 4) {
                last_key_state = 1;
                input_buffer[input_index] = 1;
                display_data[input_index] = 1;
                input_index++;
            }
            if (keys[0][1] == 0 && last_key_state != 2 && input_index < 4) {
                last_key_state = 2;
                input_buffer[input_index] = 2;
                display_data[input_index] = 2;
                input_index++;
            }
            if (keys[0][2] == 0 && last_key_state != 3 && input_index < 4) {
                last_key_state = 3;
                input_buffer[input_index] = 3;
                display_data[input_index] = 3;
                input_index++;
            }
            if (keys[1][0] == 0 && last_key_state != 4 && input_index < 4) {
                last_key_state = 4;
                input_buffer[input_index] = 4;
                display_data[input_index] = 4;
                input_index++;
            }
            if (keys[1][1] == 0 && last_key_state != 5 && input_index < 4) {
                last_key_state = 5;
                input_buffer[input_index] = 5;
                display_data[input_index] = 5;
                input_index++;
            }
            if (keys[1][2] == 0 && last_key_state != 6 && input_index < 4) {
                last_key_state = 6;
                input_buffer[input_index] = 6;
                display_data[input_index] = 6;
                input_index++;
            }
            if (keys[2][0] == 0 && last_key_state != 7 && input_index < 4) {
                last_key_state = 7;
                input_buffer[input_index] = 7;
                display_data[input_index] = 7;
                input_index++;
            }
            if (keys[2][1] == 0 && last_key_state != 8 && input_index < 4) {
                last_key_state = 8;
                input_buffer[input_index] = 8;
                display_data[input_index] = 8;
                input_index++;
            }
            if (keys[2][2] == 0 && last_key_state != 9 && input_index < 4) {
                last_key_state = 9;
                input_buffer[input_index] = 9;
                display_data[input_index] = 9;
                input_index++;
            }
            if (keys[3][1] == 0 && last_key_state != 0 && input_index < 4) {
                last_key_state = 0;
                input_buffer[input_index] = 0;
                display_data[input_index] = 0;
                input_index++;
            }
            
            // Кнопка # - перевірка паролю
            if (keys[3][2] == 0 && last_key_state != 11 && input_index == 4) {
                last_key_state = 11;
                checkPassword();
            }
        }
        
        // Скидання стану кнопок
        if (keys[0][0] == 1 && last_key_state == 1) last_key_state = 12;
        if (keys[0][1] == 1 && last_key_state == 2) last_key_state = 12;
        if (keys[0][2] == 1 && last_key_state == 3) last_key_state = 12;
        if (keys[1][0] == 1 && last_key_state == 4) last_key_state = 12;
        if (keys[1][1] == 1 && last_key_state == 5) last_key_state = 12;
        if (keys[1][2] == 1 && last_key_state == 6) last_key_state = 12;
        if (keys[2][0] == 1 && last_key_state == 7) last_key_state = 12;
        if (keys[2][1] == 1 && last_key_state == 8) last_key_state = 12;
        if (keys[2][2] == 1 && last_key_state == 9) last_key_state = 12;
        if (keys[3][0] == 1 && last_key_state == 10) last_key_state = 12;
        if (keys[3][1] == 1 && last_key_state == 0) last_key_state = 12;
        if (keys[3][2] == 1 && last_key_state == 11) last_key_state = 12;
        
        // Оновлення лічильників
        main_counter++;
        
        // Таймер 5 хвилин (1 секунда)
        if (timer_running && main_counter >= 100) {
            main_counter = 0;
            timer_counter++;
            if (timer_counter >= 10) {
                timer_counter = 0;
                updateTimer();
            }
        }
        
        // Зсув останніх 4 цифр (500 мс)
        if (!password_mode && !password_verified && !timer_running) {
            shift_counter++;
            if (shift_counter >= 50) {
                shift_counter = 0;
                shiftRightLast4();
            }
        }
        
        // Мигання для індикації паролю (250 мс)
        if (password_verified) {
            blink_counter++;
            if (blink_counter >= 25) {
                blink_counter = 0;
                blink_state = !blink_state;
            }
            
            // Автоматичний вихід через 5 секунд
            static uint32_t indication_timer = 0;
            indication_timer++;
            if (indication_timer >= 500) {
                indication_timer = 0;
                password_verified = 0;
                // Відновити початковий стан
                uint8_t default_data[8] = {0, 0, 0, 0, 5, 4, 0, 0};
                for (int i = 0; i < 8; i++) {
                    display_data[i] = default_data[i];
                }
            }
        }
        
        CyDelay(10);
    }
}