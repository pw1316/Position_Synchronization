#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

package strtopkg(char* ptr, char** endptr, int base){
	package pkg;
	char *p;

	memset(&pkg, 0, sizeof(pkg));
	if(!ptr) return pkg;
	pkg.position.x = strtol(ptr, &p, base);
	pkg.position.y = strtol(p, &p, base);
	pkg.velocity.x = strtol(p, &p, base);
	pkg.velocity.y = strtol(p, &p, base);
	if(endptr) *endptr = p;
	return pkg;
}

void package_print(package pkg){
	printf("PKG:POSITION(%ld,%ld) VELOCITY(%ld,%ld)\n", pkg.position.x, pkg.position.y, pkg.velocity.x, pkg.velocity.y);
}

package_ptr package_create_queue(){
	package_ptr head;
	head = (package_ptr)malloc(sizeof(package_node));
	if(!head) return NULL;
	memset(head, 0, sizeof(package_node));
	head->next = NULL;
	return head;
}

void package_destroy_queue(package_ptr head){
	package_ptr p = head;
	while(head){
		p = head;
		head = head->next;
		free(p);
	}
}

int package_node_enqueue(package_ptr head, package *pkgptr){
	package_ptr p, q;
	if(!head) return -1;
	p = head;
	while(p->next) p = p->next;
	q = (package_ptr)malloc(sizeof(package_node));
	if(!q) return -2;
	q->pkg = *pkgptr;
	q->next = NULL;
	p->next = q;
	return 0;
}

int package_node_dequeue(package_ptr head, package *pkgptr){
	package_ptr p, q;
	if(!head) return -1;
	if(!head->next) return -2;
	p = head->next;
	q = p->next;
	head->next = q;
	*pkgptr = p->pkg;
	free(p);
	return 0;
}

void package_print_queue(package_ptr head){
	if(!head){
		printf("Queue not exists!\n");
	}
	else{
		package_ptr p = head->next;
		if(!p){
			printf("Empty queue!\n");
		}
		else{
			printf("Head->");
			while(p){
				if(p->next) printf("[(%ld,%ld),(%ld,%ld)]->", p->pkg.position.x, p->pkg.position.y, p->pkg.velocity.x, p->pkg.velocity.y);
				else printf("[(%ld,%ld),(%ld,%ld)]\n", p->pkg.position.x, p->pkg.position.y, p->pkg.velocity.x, p->pkg.velocity.y);
				p = p->next;
			}
		}
	}
}

#ifdef DEBUG

int main(){
	package_ptr head = package_create_queue();
	if(!head){
		printf("Create queue failed!\n");
		return 1;
	}
	package pkg1 = strtopkg("0 0 0 0", NULL, 10);
	package pkg2 = strtopkg("2 2 2 2", NULL, 10);
	package_print(pkg1);
	package_print(pkg2);
	package_node_enqueue(head, &pkg1);
	package_node_enqueue(head, &pkg2);
	package_print_queue(head);
	package_node_dequeue(head,&pkg2);
	package_print(pkg1);
	package_print(pkg2);
	package_print_queue(head);
	package_destroy_queue(head);
	head = NULL;
	package_print_queue(head);
	head = package_create_queue();
	package_print_queue(head);
}

#endif