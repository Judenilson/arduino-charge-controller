/*
Controlador de Carga
Projeto Criado por Judenilson Araujo Silva, a pedido de seu amigo Dennis Rego de Farias Sousa,
para controle de uso do inversor 12v/220v em sua residência, controlando o estado de acionamento do inversor através de horas determinadas, bem como,
o desligamento automático quando as baterias estiverem abaixo da carga desejada, para conservação das mesmas, permitindo o religamento apenas
quando a voltagem de carga tiver sido alcaçada novamente.
O Arduino será alimentado pelas próprias baterias do sistema. Através de um regulador de tensão ajustado para fornecer 10V.
*/

#include <Adafruit_GFX.h>     // Biblioteca para uso no display
#include <Adafruit_SSD1306.h> // Biblioteca para uso no display

#include <Wire.h>   // Biblioteca para I2C com o módulo RTC (Real Time Clock)
#include <RTClib.h> // Biblioteca do RTC (RTC_DS3231 no meu caso)  

#include <EEPROM.h> // Biblioteca para trabalhar com a memória EEPROM

// Declarações usadas no Real Time Clock (RTC) ------------------------------------------------------------------------------
RTC_DS3231 rtc; // Criando um Objeto do tipo RTC_DS3231
char diasDaSemana[7][11] = {"  Domingo", "  Segunda", "    Terca", "   Quarta", "   Quinta", "    Sexta", "   Sabado"};
char meses[12][4] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
int tempo[3]={0,0,0};

// Declarações usadas no display ------------------------------------------------------------------------------
// #define SCREEN_WIDTH 128 // OLED display width, in pixels
// #define SCREEN_HEIGHT 64 // OLED display height, in pixels
// #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display (-1);

// Declarações usadas na medição de tensão ----------------------------------------------------------------
const int pinTensaoIn = A3; // Pino analógico do arduino para medição de tensão
float tensaoMedida = 0.0;
int ajusteSensor = 0; // (2 bytes), Variável usada para possibilitar algum ajuste na medição quando instalado em baterias e conferido com voltímetro.
int timer[8]={23, 59, 2, 2, 3, 3, 4, 4}; // (16 bytes)
float tensaoRefInf = 12.15f; // (4 bytes) Variável de referência inferior para desarme do sistema.
float tensaoRefSup = 13.05f; // (4 bytes) Variável de referência superior para possibilitar rearme do sistema.
float rearme = true; // Variável para controlar se o sistema ja descarregou as baterias.
uint32_t timer_tensao_baixa = 0;
// A EEPROM vai armazenar 26 Bytes, no Arduino UNO seu tamanho disponível é de 1KB, ou seja, dá q sobra!

// Declarações usadas nos botões ----------------------------------------------------------------
#define btMenu 8
#define btMais 9
#define btMenos 10

// Declarações usadas nos relés ----------------------------------------------------------------
// O ideal é q os relés sejam de acionamento no estado alto (HIGH) não é o caso desse projeto.
#define releUm 12
#define releDois 11
bool estadoReleA = true;
bool estadoReleB = true;

int menu = 0; // declaração de seleção do menu

void setup(){
	// as definições dos relés vem no primeiro instante para que ele não tenha tempo de acionamento, antes da saida ficar HIGH. 
	pinMode(releUm, OUTPUT);
	pinMode(releDois, OUTPUT);	
	digitalWrite(releUm, estadoReleA);
	digitalWrite(releDois, estadoReleB);

	Serial.begin(115200);
	delay(1000);

	// Para Limpar a EPROM. Rodar apenas uma vez.
  // for (int i = 0 ; i < 50 ; i++) {
  // // EEPROM.write(i, 0); // Limpar EEPROM colocando zero em todas as posições
  // }

	pinMode(btMenu, INPUT);
	pinMode(btMais, INPUT);
	pinMode(btMenos, INPUT);
	pinMode(pinTensaoIn, INPUT);

	// iniciando o Display nas portas padrão para comunicação I2C, display 128x32, o 128x64 é 0x3D
	// Display OLED e Arduino UNO, pinos -> SDA pino A4, SCK pino A5, GND e VCC são óbvios :P  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.print("Iniciando...");
  display.display();

  delay(1500);
  // SETUP RTC ------------------------------------------------------------------------------
  if (!rtc.begin()) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,5);
      display.print("RTC Problema!");
      display.display();
      while (1);
    }
  if(rtc.lostPower()){ //Se remover a bateria o relógio será ajustado para o dia final do desenvolvimento.
    display.clearDisplay(); 
    display.setCursor(0,10);
    display.println("Ajuste a");     
    display.setCursor(0,20); 
    display.println("HORA!");  
    display.display();
    rtc.adjust(DateTime(2020, 4, 11, 12, 0, 0)); //(ANO), (MÊS), (DIA), (HORA), (MINUTOS), (SEGUNDOS)
    delay(1500);
  }

  // Resgatando as configurações salvas na EEPROM, caso o Arduino seja desligado.
	EEPROM.get(0, ajusteSensor);
	for (int i = 0; i < 9; i++){
		EEPROM.get(2 + i * 2, timer[i]);
	}	
	EEPROM.get(18, tensaoRefInf);
	EEPROM.get(22, tensaoRefSup);
}

void loop(){
  int leituraSensor = analogRead(pinTensaoIn);
  tensaoMedida = (((leituraSensor * 5.0) + ajusteSensor) / 1024.0) / (7539.0 / (29960.0 + 7539.0)) ; // cálculo para exibir tensão de alimentação. 
  // ((Leitura_Sensor * Tensão_Máxima_da_Porta) / Resolução) / ((R1 / (R2 + R1)) + ajustes);
	
	if (digitalRead(btMais) && digitalRead(btMenos)){
  	delay(1000);
  	ajustarDataHora();
	}

  DateTime now = rtc.now();  
  tempo[0] = now.hour();
  tempo[1] = now.minute();
  tempo[2] = now.second();
	reles();
  // Dia da semana por extenso  --------------------------------------------
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(1,0);
  display.print(diasDaSemana[now.dayOfTheWeek()]);
  display.print(" ");
  // Dia, Mês e Ano (17-OUT-2019) ------------------------------------------
  if (now.day() < 10){  // Colocar o zero na frente quando tiver 1 digito apenas
    display.print(0);
    display.print(now.day(), DEC);
  }else{
    display.print(now.day(), DEC);
  }
  display.print(".");
  display.print(meses[now.month()-1]);
  display.print(".");
  display.println(now.year(), DEC);
  // Horas ------------------------------------------------------------------
  display.setCursor(80,8);
  if (now.hour() < 10){ // Colocar o zero na frente quando tiver 1 digito apenas
    display.print(0);
    display.print(now.hour(), DEC);
  }else {
    display.print(now.hour(), DEC);
  }
  display.print(":");
	// MINUTOS ----------------------------------------------------------------
  if (now.minute() < 10){ // Colocar o zero na frente quando tiver 1 digito apenas
    display.print(0);
    display.print(now.minute(), DEC);
  }else{
    display.print(now.minute(), DEC);
  }  
  display.print(":");  
  // SEGUNDOS ---------------------------------------------------------------
  if (now.second() < 10){ // Colocar o zero na frente quando tiver 1 digito apenas
    display.print(0);
    display.print(now.second(), DEC);
  }else{
    display.print(now.second(), DEC);
  }

	if (digitalRead(btMenu)){
		menu++;
		if (menu > 0){
			menu = 0;
		}
	}

	switch (menu){
		case 0:
		  display.setTextSize(2);
		  display.setCursor(0,12); 
		  display.print(tensaoMedida); 
		  display.print("V");
		break;
		case 1: // Timer A Setando HORA ON
			setTimer("Ligar 1", 1, 0, 1);
		break;
		case 2: // Timer A Setando MINUTO ON
			setTimer("Ligar 1", 0, 0, 1);
		break;
		case 3: // Timer A Setando HORA OFF
			setTimer("Desligar 1", 1, 2, 3);
		break;
		case 4: // Timer A Setando MINUTO OFF
			setTimer("Desligar 1", 0, 2, 3);
		break;
		case 5: // Timer B Setando HORA ON
			setTimer("Ligar 2", 1, 4, 5);
		break;
		case 6: // Timer B Setando MINUTO ON
			setTimer("Ligar 2", 0, 4, 5);
		break;
		case 7: // Timer B Setando HORA OFF
			setTimer("Desligar 2", 1, 6, 7);
		break;
		case 8: // Timer B Setando MINUTO OFF
			setTimer("Desligar 2", 0, 6, 7);
		break;
		case 9: // Setando Tensão de Referência Inferior
			display.setTextSize(1);
			display.setCursor(0,8); 
			display.print("Ref. Inf.");
			display.setTextSize(2);
			display.setCursor(0,16); 
			if (digitalRead(btMais) == HIGH){
				tensaoRefInf += 0.05;
				if (tensaoRefInf >= (tensaoRefSup - 0.2)){	// Ajuste de tensão para não cruzar o limite superior
					tensaoRefInf = tensaoRefSup - 0.2;
				}
			}
			if (digitalRead(btMenos) == HIGH){
				tensaoRefInf -= 0.05;
				if (tensaoRefInf <= 0.0){
					tensaoRefInf = 0.0;
				}
			}
			display.print(tensaoRefInf);
			display.print("V");
		break;
		case 10: // Setando Tensão de referência Superior
			display.setTextSize(1);
			display.setCursor(0,8); 
			display.print("Ref. Sup.");
			display.setTextSize(2);
			display.setCursor(0,16); 			
			if (digitalRead(btMais) == HIGH){
				tensaoRefSup += 0.05;
				if (tensaoRefSup > 25.0){ //limite de tensão que pode ser medida com o dividor instalado, acima disso, danificará o Arduino.
					tensaoRefSup = 25.0;
				}
			}
			if (digitalRead(btMenos) == HIGH){
				tensaoRefSup -= 0.05;
				if (tensaoRefSup <= (tensaoRefInf + 0.2)){ 	// Ajuste de tensão para não cruzar o limite inferior
					tensaoRefSup = tensaoRefInf + 0.2;
				}
			}			
			display.print(tensaoRefSup);
			display.print("V");
		break;
		case 11:
		  display.setTextSize(2);
		  display.setCursor(0,12); 
		  display.print(tensaoMedida); 
		  display.print("V");

			display.setTextSize(1);
			display.setCursor(73,16); 
			display.print("Ajuste DC");
			display.setCursor(100,24); 
			if (digitalRead(btMais) == HIGH){
				ajusteSensor++;
			}
			if (digitalRead(btMenos) == HIGH){
				ajusteSensor--;
			}		
			display.print(ajusteSensor);
		break;
		case 12:
			display.setTextSize(1);
			display.setCursor(0,8); 
			display.println("Salvando");
			display.println("Ajustes...");			
			display.display();
			salvarEEPROM();
			menu = 0;
	}
  display.display(); // Exibir tudo no display
}

void salvarEEPROM(){
  EEPROM.put(0, ajusteSensor);
  for (byte i = 0; i < 9; i++){  	
  	EEPROM.put(2 + i * 2, timer[i]);
  }
  EEPROM.put(18, tensaoRefInf);
  EEPROM.put(22, tensaoRefSup);
}

void setTimer(String titulo, int tipo, int hora, int minuto){
	display.setTextSize(1);
  display.setCursor(0,8); 
  display.print(titulo);
  display.setTextSize(2);
  display.setCursor(0,16); 	
	if(timer[hora]<10){
  	display.print(0);
  } 
  display.print(timer[hora], DEC);
  display.print(":");
  if(timer[minuto]<10){
  	display.print(0);
  } 
  display.print(timer[minuto], DEC);
  display.setTextSize(1);
  display.setCursor(0,25); 
 	if (tipo == 1){	// tipo 1 se for ajuste de hora e 0 se for minuto.
	  display.print("____");
	  if (digitalRead(btMais) == HIGH){
			timer[hora]++;
			if (timer[hora] > 23){
				timer[hora] = 0;
			}
		}
		if (digitalRead(btMenos) == HIGH){
			timer[hora]--;
			if (timer[hora] < 0){
				timer[hora] = 23;
			}
		}
	}else{
	  display.print("      ____");
	  if (digitalRead(btMais) == HIGH){
			timer[minuto]++;
			if (timer[minuto] > 59){
				timer[minuto] = 0;
			}
		}
		if (digitalRead(btMenos) == HIGH){
			timer[minuto]--;
			if (timer[minuto] < 0){
				timer[minuto] = 59;
			}
		}
	}
}

void ajustarDataHora(){	
	DateTime now = rtc.now();
	int relogio[6] = {now.year(),now.month(),now.day(),now.hour(),now.minute(),now.second()};
	int menu = 0;
	String titulo = "";
	while (!(digitalRead(btMais)==true && digitalRead(btMenos)==true)){  		
		if (digitalRead(btMenu)){
			menu++;
			if (menu > 5){ menu = 0; }
		}
		switch(menu){
			case 0:
				titulo = "Ajuste o Dia";
				if (digitalRead(btMais)){ relogio[2]++; if (relogio[2] > 31){ relogio[2] = 1; }}			
				if (digitalRead(btMenos)){ relogio[2]--; if (relogio[2] < 1){ relogio[2] = 31; }}				
			break;
			case 1:
				titulo = "Ajuste o Mes";
				if (digitalRead(btMais)){ relogio[1]++; if (relogio[1] > 12){ relogio[1] = 1; }}			
				if (digitalRead(btMenos)){ relogio[1]--; if (relogio[1] < 1){ relogio[1] = 12; }}
			break;
			case 2:
				titulo = "Ajuste o Ano";
				if (digitalRead(btMais)){ relogio[0]++; if (relogio[0] > 2100){ relogio[0] = 2020; }}			
				if (digitalRead(btMenos)){ relogio[0]--; if (relogio[0] < 2020){ relogio[0] = 2100; }}
			break;
			case 3:
				titulo = "Ajuste a Hora";
				if (digitalRead(btMais)){ relogio[3]++; if (relogio[3] > 23){ relogio[3] = 0; }}			
				if (digitalRead(btMenos)){ relogio[3]--; if (relogio[3] < 0){ relogio[3] = 23; }}
			break;
			case 4:
				titulo = "Ajuste os Minutos";
				if (digitalRead(btMais)){ relogio[4]++; if (relogio[4] > 59){ relogio[4] = 0; }}			
				if (digitalRead(btMenos)){ relogio[4]--; if (relogio[4] < 0){ relogio[4] = 59; }}
			break;
			case 5:
				titulo = "Ajuste os Segundos";
				if (digitalRead(btMais)){ relogio[5]++; if (relogio[5] > 59){ relogio[5] = 0; }}			
				if (digitalRead(btMenos)){ relogio[5]--; if (relogio[5] < 0){ relogio[5] = 59; }}
			break;
		}		
		// Dia da semana por extenso  --------------------------------------------
	  display.clearDisplay();
	  display.setTextSize(1);
	  display.setCursor(0,0);
	  display.println(titulo);
	  // Dia, Mês e Ano (17-OUT-2019) ------------------------------------------
	  if (relogio[2] < 10){  // Colocar o zero na frente quando tiver 1 digito apenas
	    display.print(0);
	    display.print(relogio[2], DEC);
	  }else{
	    display.print(relogio[2], DEC);
	  }
	  display.print(".");
	  display.print(meses[relogio[1]-1]);
	  display.print(".");
	  display.println(relogio[0], DEC);
	  // Horas ------------------------------------------------------------------	  
	  if (relogio[3] < 10){ // Colocar o zero na frente quando tiver 1 digito apenas
	    display.print(0);
	    display.print(relogio[3], DEC);
	  }else {
	    display.print(relogio[3], DEC);
	  }
	  display.print(":");
		// MINUTOS ----------------------------------------------------------------
	  if (relogio[4] < 10){ // Colocar o zero na frente quando tiver 1 digito apenas
	    display.print(0);
	    display.print(relogio[4], DEC);
	  }else{
	    display.print(relogio[4], DEC);
	  }  
	  display.print(":");  
	  // SEGUNDOS ---------------------------------------------------------------
	  if (relogio[5] < 10){ // Colocar o zero na frente quando tiver 1 digito apenas
	    display.print(0);
	    display.print(relogio[5], DEC);
	  }else{
	    display.print(relogio[5], DEC);
	  }
	  display.display();
  }
	rtc.adjust(DateTime(relogio[0], relogio[1], relogio[2], relogio[3], relogio[4], relogio[5])); //(ANO), (MÊS), (DIA), (HORA), (MINUTOS), (SEGUNDOS)
	delay(1000);
}

void reles(){  
	DateTime time_now = rtc.now(); // Criando um ogjeto para pegar o tempo do RTC no momento.
	uint32_t timeStamp = time_now.unixtime(); // Pegando o tempo no formato Unix.
	if (tensaoMedida < tensaoRefInf){	// Se a tensão estiver abaixo do limite inferior, desligará os relés.		
		if (timeStamp - timer_tensao_baixa >= 10){	
			// Se tiver passado 10s desde que a queda da tensão tenha ficado abaixo da referência, ele desarma os relés.
			// Isso serve para que evite o desligamento inadequado do inversor, quando a tensão flutuar abaixo da referência devido uma carga inicial,
			// que após estabilizada fique acima do esperado na referência de tensão de corte.
			estadoReleA = true;
			estadoReleB = true;
	  	digitalWrite(releUm, estadoReleA);
	  	digitalWrite(releDois, estadoReleB);
	  	rearme = false;	// Trava para que o sistema só volte a rearmar após atingir o limite superior de tensão.
	  	return;
	  }
	}	else {
		timer_tensao_baixa = timeStamp; // Caso a tensão não esteja abaixo do corte, o timer é atualizado com o "tempo agora". 
	}

	if (tensaoMedida >= tensaoRefSup){		// Assim q o sistema detectar a tensão no limite superior vai rearmar.
		rearme = true;
	}
	if (rearme){
		if ((tempo[0] == timer[0]) && (tempo[1] == timer[1])){	// Ligar relé 1
			estadoReleA = false;		
	  	digitalWrite(releUm, estadoReleA);
		}	
		if ((tempo[0] == timer[2]) && (tempo[1] == timer[3])){	// Desligar relé 1
			estadoReleA = true;		
	  	digitalWrite(releUm, estadoReleA);
		}
		if ((tempo[0] == timer[4]) && (tempo[1] == timer[5])){	// Ligar relé 2
			estadoReleB = false;		
	  	digitalWrite(releDois, estadoReleB);
		}	
		if ((tempo[0] == timer[6]) && (tempo[1] == timer[7])){	// Desligar relé 2
			estadoReleB = true;		
	  	digitalWrite(releDois, estadoReleB);
		}
	}
}