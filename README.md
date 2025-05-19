# Estação de Alerta de Enchente com Simulação por Joystick

## Objetivo Geral

O objetivo do projeto é implementar um sistema que represente uma estação de alerta de enchente, onde os dados de nível da água nos rios e o volume de chuva serão simulados pelas leituras analógicas do joystick. Caso haja uma situação de alerta, o LED RGB, o buzzer e a matriz de LEDs são ativados para indicar o perigo. Além disso, o display OLED exibe continuamente as informações capturadas e o modo atual do sistema.

## Descrição Funcional

O sistema utiliza o FreeRTOS como sistema operacional para gerenciar tarefas concorrentes. A lógica central consiste na leitura dos dados simulados via joystick e no envio dessas leituras para diferentes tarefas por meio de filas.

- O eixo **X** do joystick representa o **volume de chuva**.
- O eixo **Y** do joystick representa o **nível de água**.
- Ambos os valores são normalizados de 0 a 100 e enviados para **filas separadas** para cada componente (Display, LED, Buzzer, Matriz de LEDs).

### Lógica de Alerta

Se:
- **Nível de água ≥ 70%** ou
- **Volume de chuva ≥ 80%**

Então o sistema entra em **Modo Alerta**. Caso contrário, permanece em **Modo Normal**.

Cada componente recebe sua informação de modo através de sua respectiva fila e reage da seguinte forma:

### Modo Normal

- **Display**: Mostra os valores normalizados de chuva e água, com a mensagem `Modo: Normal`.
- **LED RGB**: Permanece desligado.
- **Matriz de LEDs**: Permanece desligada.
- **Buzzer**: Permanece desligado.

### Modo Alerta

- **Display**: Mostra os valores normalizados com a mensagem `Modo: ALERTA!!`, piscando as bordas do display.
- **LED RGB**: Acende na cor **vermelha**.
- **Matriz de LEDs**: Exibe um **losango vermelho com interior amarelo**, simbolizando perigo.
- **Buzzer**: Alterna entre **ligado e desligado a cada 100 ms**, produzindo um som de alerta contínuo.

## Uso dos Periféricos da Placa BitDogLab

| Componente      | Função no Projeto                                      |
|------------------|--------------------------------------------------------|
| **Joystick Eixo X** | Simula o **volume de chuva**                         |
| **Joystick Eixo Y** | Simula o **nível de água**                           |
| **LED RGB**         | Indica visualmente o alerta na cor **vermelha**     |
| **Matriz de LEDs 5x5** | Exibe **ícone de perigo** em forma de losango      |
| **Buzzer A**         | Emite som de alerta quando em modo de perigo       |
| **Display OLED**     | Exibe os dados normalizados e o modo atual do sistema |

## Estrutura do Código

O código está organizado em múltiplas tarefas FreeRTOS, cada uma responsável por controlar um periférico ou coletar dados. Todas as tarefas se comunicam via **filas**, sem uso de semáforos ou mutexes, conforme exigido pelas especificações do projeto.
