// 08_CoffeeMaker Turn on and off the motors for liquid dispensing
// Using pins 5,6 as output pins to control motors

// Global Declarations

////// For RFID
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::StatusCode status;
MFRC522::MIFARE_Key key = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // 6 bytes, using default key for authentication

////// For LCD
#include <Wire.h>  // LCD
#include <LiquidCrystal_PCF8574.h>
#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above
LiquidCrystal_PCF8574 lcd(0x27); // Instantiate the lcd as a global object and set the address to 0x27

////// For Buttons
#define Pin_Sugars A1
#define Pin_Ok A2
#define Pin_Creams A3
#define SwitchON LOW
#define SwitchOFF HIGH

////// For dispense()
const byte MC_1 = 5; // Pin 5 PWM //Sugar
const byte MC_2 = 6; // Pin 6 PWM // Cream

///// User Info
struct USER_INFO {  // To simplify implementation, lets make this in multiples of 16 bytes (RFID blocks).
  // Structure containing user's name and sugar/cream preferences
  char name_chars[16];           // The name fields occupies one block (16 bytes)
  int16_t total_cups_consumed;   // These fields occupy a second block (total 16 bytes)
  int16_t total_sugars_consumed;
  int16_t total_creams_consumed;
  int8_t preferred_num_sugars;
  int8_t preferred_num_creams;
  int8_t unused[8];
};
USER_INFO user_info;


void setup() {
  Serial.begin(9600);

  // For RFID
  SPI.begin();
  mfrc522.PCD_Init();    // proximity coupling device (PCD)
  delay_message("Bootup: ", 4);  // show bootup count in console

  // For LCD
  lcd.begin(20, 4);                     // initialize the lcd for a 20 char, 4 line display
  lcd.setBacklight(1); // 0:Off, 1:On
  //lcd.setCursor(2,1); // (x,y) Position index, starting with (0,0)
  //lcd.print("Hello, Alexander!");
  //delay(2000);
  lcd.clear();

  lcd.setCursor(0,0); lcd.print("Bootup: ");
  delay(250);
  lcd.setCursor(8,0); lcd.print("1");
  delay(250);
  lcd.setCursor(9,0); lcd.print(",2");
  delay(250);
  lcd.setCursor(11,0); lcd.print(",3");
  delay(250);
  lcd.setCursor(13,0); lcd.print(",4");
  delay(1000);
  lcd.clear();
  //lcd.setCursor(0,0); lcd.print("Place cup here");

  // For Buttons
  pinMode(Pin_Sugars, INPUT_PULLUP); // Links Button to Pin_Sugar
  pinMode(Pin_Ok, INPUT_PULLUP); // Links Button to Pin_Ok
  pinMode(Pin_Creams, INPUT_PULLUP); // Links Button to Pin_Creams

  // For dispense()
  pinMode(MC_1, OUTPUT); // Links Motor 1 to pin
  pinMode(MC_2, OUTPUT); // Links Motor 2 to pin

}

void loop() {

  // Wait for RFID
  rfid_wait();


  // Load Preferences from RFID
  init_user_info();  // Init to 0.

  
  rfid_load();


  // Program New Cup
  if (0) {
    rfid_program("Ryan"); // Names longer than 16 chars will be automatically truncated.
  }

  // Welcome message
  welcome_msg();

  // User Inputs for changes to prefences
  button_press();


  // Save preferences to RFID
  rfid_save(); 


  // Halt RFID
  rfid_halt();

  
  // Control Motors to dispense
  dispense();


  //while (1) { // debug
    //delay(1000);
  //}



} // End of loop

byte rfid_wait() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Place cup here");
  while (1) {
    if ( ! mfrc522.PICC_IsNewCardPresent()) // If no card present,
      continue;  // Keep looping, else proceed below

    if ( ! mfrc522.PICC_ReadCardSerial()) // If card can't be read (broken)
      continue;  // Keep looping, else proceed below

    // At this point card is present and the serial number can be read (UID)

    // Show card UID
    if (0) {
      Serial.print("Card UID: ");
      dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // Prints in hex code the UID in the array in byte char corresponding to numbers
      Serial.print("\n\n");
    }
    return 0;
  } // while loop
  return 0;
} // rfid_wait()



byte rfid_program(char name_chars[]) {
  // Program New Cup
  //Serial.print("sizeof(name_chars): "); Serial.println(sizeof(name_chars));
  for (byte k = 0; k < 16; k++) {
    if (k < (sizeof(name_chars) - 1)) // The -1 is to omit the trailing \0 character added by "" string operator
      user_info.name_chars[k] = name_chars[k];  // copy over name characters
    else
      user_info.name_chars[k] = ' ';  // fill rest  with space characters
  }
  //user_info.preferred_num_sugars = 2; //debug

  rfid_save(); // Save the user_info struct to RFID tag

}// rfid_program()



byte rfid_load() {
  // Read RFID tag

  byte read_buffer[18];  // 18 is a reqired size
  byte n_bytes;
  byte k_block;

  rfid_auth();   // required to authenticate before accessing a sector

  // Read block (1 of 2)
  if (1) {
    k_block = 1;   // required to read entire block of 16 bytes, one block at a time
    n_bytes = 18;  // 18 is the reqired size, even though we are reading 16 bytes at a time
    status = mfrc522.MIFARE_Read(k_block, read_buffer, &n_bytes);  // note the use of "&" for read but not for write
    if (status != MFRC522::STATUS_OK) {
      Serial.print("MIFARE_Read() block 1 failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));

      init_user_info();
    }
    else {
      Serial.println("MIFARE_Read() block 1 succeded");

      // Copy the temp buffer to our global struct "user_info"
      for (byte k = 0; k < 16; k++) {
        user_info.name_chars[k] = read_buffer[k];
      }
    }
    Serial.println();
  } // read block (1 of 2)

  // Read block (2 of 2)
  if (1) {
    k_block = 2;   // required to read entire block of 16 bytes, one block at a time
    n_bytes = 18;  // 18 is the reqired size, even though we are reading 16 bytes at a time
    status = mfrc522.MIFARE_Read(k_block, read_buffer, &n_bytes);  // note the use of "&" for read but not for write
    if (status != MFRC522::STATUS_OK) {
      Serial.print("MIFARE_Read() block 2 failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));

      init_user_info();
    }
    else {
      Serial.println("MIFARE_Read() block 2 succeded");

      // Copy the temp buffer to our global struct "user_info"
      byte *p;  // Will use this pointer for the block
      p = &(user_info.name_chars[0]);  // Note, p is a pointer to size byte not size user_info
      //Serial.print("p: ");Serial.println((int) p);  // print value of pointer address
      p = p + 16; // Start byte copy after the "name_chars" field, 16 bytes from start of user_info struct
      //Serial.print("p: ");Serial.println((int) p);  // print value of pointer address
      for (byte k = 0; k < 16; k++) {
        //p[k] = read_buffer[k];
        *p = read_buffer[k];
        p = p + 1;
      }
    }
    Serial.println();
  } // read block (2 of 2)
} // rfid_load()



void lcd_boot_up(){
  
}

void welcome_msg(){
    // Display name
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Welcome");
  lcd.setCursor(0,1); lcd.print(user_info.name_chars);
  lcd.setCursor(9,2); lcd.print("^_^");
  
  delay(2000);
}

void lcd_show_pref(){  // Show preferences stored in global user_info onto LCD
  // Check for errors
  if(user_info.preferred_num_sugars > 3){
    Serial.println("Error: Too many sugars on display");
  }
  if(user_info.preferred_num_sugars < 0){
    Serial.println("Error: Sugars can't be negative on display");
  }
  if(user_info.preferred_num_creams > 3){
    Serial.println("Error: Too many creams on display");
  }
  if(user_info.preferred_num_creams < 0){
    Serial.println("Error: Creams can't be negative on display");
  }


  // Display preferences
  lcd.clear();

  // Write column of numbers 0,1,2,3 in center of screen
  int x_mid = 10;
  
  lcd.setCursor(x_mid,0); lcd.print("3");
  lcd.setCursor(x_mid,1); lcd.print("2");
  lcd.setCursor(x_mid,2); lcd.print("1");
  lcd.setCursor(x_mid,3); lcd.print("0");
  // Write sugar and cream on row of preference
  int x_sugar = 2;  // starting x pos for the word sugar>
  int x_cream = 12; // Staring x pos for the word <cream
  int y_sugar = 3 - user_info.preferred_num_sugars;  // starting y pos for the word sugar>
  int y_cream = 3 - user_info.preferred_num_creams; // Staring y pos for the word <cream
  
  lcd.setCursor(x_sugar, y_sugar); lcd.print("Sugars>");
  lcd.setCursor(x_cream, y_cream); lcd.print("<Creams");

} // lcd_show_pref()
  

byte button_press() {
  // put your main code here, to run repeatedly:
  byte switch_val_Sugars;
  byte switch_val_Ok;
  byte switch_val_Creams;
  byte switch_val_Sugar_prev;
  byte switch_val_Cream_prev;
  
  lcd_show_pref();

  while (1) {

    switch_val_Sugars = digitalRead(Pin_Sugars); // Reads sugar switch
    if (switch_val_Sugar_prev == SwitchOFF && switch_val_Sugars == SwitchON) {
      user_info.preferred_num_sugars += 1;
      
      if (user_info.preferred_num_sugars > 3) { // Wrap around sugar
        user_info.preferred_num_sugars = 0;
      }
      Serial.println(user_info.preferred_num_sugars);
      lcd_show_pref();

    }
    switch_val_Sugar_prev = switch_val_Sugars;
    delay(50);  // Delay by 1 second
    //Serial.print("preffered_Sugars: "); //
    //Serial.println(user_info.preffered_num_sugars);

    
    switch_val_Creams = digitalRead(Pin_Creams); // Reads cream switch
    if (switch_val_Cream_prev == SwitchOFF && switch_val_Creams == SwitchON) {
      user_info.preferred_num_creams += 1;
      if (user_info.preferred_num_creams > 3) { // Wrap around cream
        user_info.preferred_num_creams = 0;
      }

            Serial.println(user_info.preferred_num_creams);


      lcd_show_pref();

    }
    switch_val_Cream_prev = switch_val_Creams;
    delay(50);  // Delay by 1 second
    //Serial.print("preffered_Creams: "); //
    //Serial.println(user_info.preffered_num_creams);

    ////////////////////////////////////////////////////////////
    switch_val_Ok = digitalRead(Pin_Ok); // Reads Ok switch
    if (switch_val_Ok == SwitchON) {
      Serial.println("ok");
      break; // Finish reading buttons, proceed with dispensing
    }
  } // While 1
}// button_press



byte rfid_halt(){
  mfrc522.PICC_HaltA();       // Stop the RFID tag
  mfrc522.PCD_StopCrypto1();  // Stop the encryption on RFID reader
  Serial.println("Finished.");
}



byte rfid_save() {
  // Write to RFID tag

  byte n_bytes;
  byte k_block;
  byte *p;  // Will use this pointer for source of each block

  rfid_auth();  // required to authenticate before accessing a sector

  // Write block (1 of 2)
  if (1) {
    byte n_bytes = 16;  // // 16 is the reqired size
    byte k_block = 1;
    // using global struct "user_info"
    p = &(user_info.name_chars[0]);  // Beginning address of 1st block
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(k_block, p, n_bytes);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("MIFARE_Write() block 1 failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
    else {
      Serial.println("MIFARE_Write() block 1 succeded");
    }
    Serial.println();
  }

  // Write block (2 of 2)
  if (1) {
    byte n_bytes = 16;
    byte k_block = 2;
    // using global struct "user_info"
    p = &(user_info.name_chars[0]);  // Beginning address of 1st block
    p = p + 16;  // Beginning address of 2nd block
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(k_block, p, n_bytes);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("MIFARE_Write() block 2 failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
    else {
      Serial.println("MIFARE_Write() block 2 succeded");
    }
    Serial.println();
  }

} // rfid_save()



void init_user_info() { // Default everything to 0 or empty spaces
  // Copy spaces to our global struct "user_info"
  for (byte k = 0; k < 16; k++) {
    user_info.name_chars[k] = ' ';
  }
  user_info.total_cups_consumed = 0;   // These fields occupy a second block (total 16 bytes)
  user_info.total_sugars_consumed = 0;
  user_info.total_creams_consumed = 0;
  user_info.preferred_num_sugars = 0;
  user_info.preferred_num_creams = 0;
  for (byte k = 0; k < sizeof(user_info.unused); k++) {
    user_info.unused[k] = 0;
  }
} // init_user_info()



byte rfid_auth() {
  // Authenticate using key A

  Serial.print("Auth key:"); dump_byte_array(key.keyByte, 6); Serial.println();
  byte trailer_block = 3;  // We are using only the first Sector (0). The 4th block of each sector is the trailer block. Index starts at 0.
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailer_block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("PCD_Authenticate() failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else {
    Serial.println("PCD_Authenticate() succeded");

  }
  // Note that there is no need for keys unless we were also doing account balance.
  // For storing preferences, we are simply using the default keys.
} // rfid_auth()


void dump_byte_array(byte * buffer, byte bufferSize) {
  // Helper routine to dump a byte array as hex values to Serial.

  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
} // dump_byte_array()



void delay_message(char *chars, byte count) {
  Serial.print(chars);
  for (byte k = 0; k < count; k++) {
    Serial.print(" "); Serial.print(k);
    delay(1000); // delay 1 second at a time
  }
  Serial.println(" Done");
} // delay_message()



////////////////////////////////
// Control Motors to dispense //
////////////////////////////////

void dispense() {

  short ts_1 = 11000; // Time to dispense one sugar 15ml
  short tc_1 = 11000; // Time to dispense one cream 15ml
  byte PWM_s = 255; // Controls the pwm (value between 0 to 255) Sugar
  byte PWM_c = 255; // Controls the pwm (value between 0 to 255) Cream

  // Calculate the total time to dispense
  // user_info.preferred_num_sugars = 1; // debug
  // user_info.preferred_num_creams = 1; // debug
  short ts_total = ts_1 * user_info.preferred_num_sugars;
  short tc_total = tc_1 * user_info.preferred_num_creams;

  // Dispense cream and sugar
  if ( user_info.preferred_num_sugars > 0) { // Check if Sugar needs to be dispensed
    analogWrite(MC_1, PWM_s); // Turn on MC_1
    delay(ts_total); // Delay for amount of time to dispense sugar
    analogWrite(MC_1, 0); // Turn off MC_1
  }
  if ( user_info.preferred_num_creams > 0) { // Check if Cream needs to be dispensed
    analogWrite(MC_2, PWM_c); // Turn on MC_2
    delay(tc_total); // Delay for amount of time to dispense cream
    analogWrite(MC_2, 0); // Turn off MC_2
  }
}
