# Driver del tocco GSL3680

Viene dal pacchetto del costruttore della scheda JC8012P4A1C, che si scarica
da `pan.jczn1688.com` (il link sta nel manuale del display). Sta qui e non
nel registro dei componenti di Espressif perché lì non esiste: il GSL3680
non ha il firmware a bordo e se lo fa caricare via I2C a ogni accensione, e
quel firmware è legato a questo modello di vetro.

## Licenze

I tre file non hanno la stessa origine, e non hanno la stessa licenza:

| File | Origine | Licenza |
|---|---|---|
| `gsl_point_id.c`, `include/gsl_point_id.h` | Silead, l'algoritmo che riconosce le dita | **GPL-2.0-or-later**, dichiarata nell'intestazione |
| `include/esp_lcd_touch_gsl3680.h` | il costruttore: dentro c'è il firmware del chip, come tabella di numeri | **nessuna dichiarata** |
| `esp_lcd_touch_gsl3680.c` | il costruttore, sull'interfaccia `esp_lcd_touch` di Espressif; contiene anche la tabella di configurazione del chip | nessuna dichiarata |

Il firmware del chip non gira sul P4: il P4 lo copia nel controllore del
tocco all'accensione, come un sistema operativo fa con il firmware di una
scheda di rete. Il costruttore lo distribuisce pubblicamente nel suo
pacchetto per chi usa la scheda. Chi ha bisogno di una certezza che quel
pacchetto non dà deve chiederla al costruttore.

## Cosa è cambiato rispetto al pacchetto del costruttore

Solo `esp_lcd_touch_gsl3680.c`, e solo per correggere due errori trovati
sulla scheda vera. Ognuno è spiegato nel codice dove sta:

- il buffer della lettura dei tocchi era di 24 byte e la lettura ne chiede
  44: i 20 in più finivano sulla pila, sopra le variabili dell'algoritmo;
- il numero di dita riportato dal chip veniva usato come limite di due cicli
  senza controllarlo, e poteva scrivere oltre la fine dei vettori.

`gsl_point_id.c` e il firmware del chip sono come li ha distribuiti il
costruttore. Il driver vuole anche l'indirizzo I2C passato una seconda
volta in `driver_data`, altrimenti salta la sequenza di reset che lo
sceglie: quello però è dalla parte di chi lo usa, in
`firmware/main/hw_scheda.c`.
