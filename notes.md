# Puertos USB útiles

# CH9102X (actual)
/dev/ttyACM0

# CH340 (nuevo)
/dev/ttyUSB0

# Recordatorio:
# Solo cambiar el puerto al flashear


# Contexto IA

for f in main/*.[ch]; do
  echo "===== $f =====" >> dump_main.txt
  cat "$f" >> dump_main.txt
  echo -e "\n" >> dump_main.txt
done
