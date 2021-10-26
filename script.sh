for ((i=11; i<311; i++))
do
numero=0
        for ((j=0; j<=9; j++))
        do
        
        numero1=$(grep -c "r $i.$j" 40-80.tr)
        
        numero=$(($numero+$numero1))

        echo $numero >> /home/jorge/resultados/testes2021auto/pesada/hurst/40-80.txt
        
        done

done
exit 0
