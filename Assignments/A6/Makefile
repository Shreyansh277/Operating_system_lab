all :
	gcc -o cook cook_updated.c
	gcc -o waiter waiter_updated.c
	gcc -o customer customer_updated.c
	gcc -o gencustomers gencustomers.c
	./gencustomers > customers.txt


cook:
	./cook

waiter:
	./waiter

customer:
	./customer


clean :
	rm cook waiter customer gencustomers customers.txt
	
