/*
 *      Copyright (C) 2002 Fabio Fiorina
 *
 * This file is part of LIBASN1.
 *
 * LIBASN1 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LIBASN1 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


/*****************************************************/
/* File: decoding.c                                  */
/* Description: Functions to manage DER decoding     */
/*****************************************************/
 
#include <int.h>
#include <errors.h>
#include "der.h"
#include "parser_aux.h"
#include <gstr.h>
#include "structure.h"
#include "element.h"


void
_asn1_error_description_tag_error(node_asn *node,char *ErrorDescription)
{

  strcpy(ErrorDescription,":: tag error near element '");
  _asn1_hierarchical_name(node,ErrorDescription+strlen(ErrorDescription),
			  MAX_ERROR_DESCRIPTION_SIZE-40);
  strcat(ErrorDescription,"'");

}

  
unsigned long
_asn1_get_length_der(unsigned char *der,int  *len)
{
  unsigned long ans;
  int k,punt;

  if(!(der[0]&128)){
    /* short form */
    *len=1;
    return der[0];
  }
  else{
    /* Long form */
    k=der[0]&0x7F;
    punt=1;
    ans=0;
    while(punt<=k) ans=ans*256+der[punt++];
    
    *len=punt;
    return ans;
  }
}




unsigned int
_asn1_get_tag_der(unsigned char *der,unsigned char *class,int  *len)
{
  int punt,ris;

  *class=der[0]&0xE0;
  if((der[0]&0x1F)!=0x1F){
    /* short form */
    *len=1;
    ris=der[0]&0x1F;
  }
  else{
    /* Long form */
    punt=1;
    ris=0;
    while(der[punt]&128) ris=ris*128+(der[punt++]&0x7F);
    ris=ris*128+(der[punt++]&0x7F);   
    *len=punt;
  }
  return ris;
}




int
_asn1_get_octet_der(unsigned char *der,int *der_len,unsigned char *str,int str_size, int *str_len)
{
  int len_len;

  if(str==NULL) return ASN1_SUCCESS;
  *str_len=_asn1_get_length_der(der,&len_len);
  if ( str_size >= *str_len)
	  memcpy(str,der+len_len,*str_len);
  else {
  	return ASN1_MEM_ERROR;
  }
  *der_len=*str_len+len_len;
  
  return ASN1_SUCCESS;
}




void
_asn1_get_time_der(unsigned char *der,int *der_len,unsigned char *str)
{
  int len_len,str_len;

  if(str==NULL) return;
  str_len=_asn1_get_length_der(der,&len_len);
  memcpy(str,der+len_len,str_len);
  str[str_len]=0;
  *der_len=str_len+len_len;
}



void
_asn1_get_objectid_der(unsigned char *der,int *der_len,unsigned char *str, int str_size)
{
  int len_len,len,k;
  char temp[20];
  unsigned long val,val1;

  if(str==NULL) return;
  len=_asn1_get_length_der(der,&len_len);
  
  val1=der[len_len]/40;
  val=der[len_len]-val1*40;

  _asn1_str_cpy(str, str_size, _asn1_ltostr(val1,temp));
  _asn1_str_cat(str, str_size, " ");
  _asn1_str_cat(str, str_size, _asn1_ltostr(val,temp));

  val=0;
  for(k=1;k<len;k++){
    val=val<<7;
    val|=der[len_len+k]&0x7F;
    if(!(der[len_len+k]&0x80)){
      _asn1_str_cat(str, str_size," ");
      _asn1_str_cat(str, str_size,_asn1_ltostr(val,temp));
      val=0;
    }
  }
  *der_len=len+len_len;
}




int
_asn1_get_bit_der(unsigned char *der,int *der_len,unsigned char *str, int str_size, int *bit_len)
{
  int len_len,len_byte;

  if(str==NULL) return ASN1_SUCCESS;
  len_byte=_asn1_get_length_der(der,&len_len)-1;
  
  if (str_size >= len_byte)
 	memcpy(str,der+len_len+1,len_byte);
  else {
  	return ASN1_MEM_ERROR;
  }
  *bit_len=len_byte*8-der[len_len];
  *der_len=len_byte+len_len+1;

  return ASN1_SUCCESS;
}




int
_asn1_extract_tag_der(node_asn *node,unsigned char *der,int *der_len)
{
  node_asn *p;
  int counter,len2,len3,is_tag_implicit;
  unsigned long tag,tag_implicit=0;
  unsigned char class,class2,class_implicit=0;

  counter=is_tag_implicit=0;
  if(node->type&CONST_TAG){
    p=node->down;
    while(p){
      if(type_field(p->type)==TYPE_TAG){
	if(p->type&CONST_APPLICATION) class2=APPLICATION;
	else if(p->type&CONST_UNIVERSAL) class2=UNIVERSAL;
	else if(p->type&CONST_PRIVATE) class2=PRIVATE;
	else class2=CONTEXT_SPECIFIC;
	
	if(p->type&CONST_EXPLICIT){
	  tag=_asn1_get_tag_der(der+counter,&class,&len2);
	  counter+=len2;
	  len3=_asn1_get_length_der(der+counter,&len2);
	  counter+=len2;
	  if(!is_tag_implicit){
	    if((class!=(class2|STRUCTURED)) || (tag!=strtoul(p->value,NULL,10)))
	      return ASN1_TAG_ERROR;
	  }
	  else{    /* TAG_IMPLICIT */
	    if((class!=class_implicit) || (tag!=tag_implicit))
	      return ASN1_TAG_ERROR;
	  }

	  is_tag_implicit=0;
	}
	else{    /* TAG_IMPLICIT */
	  if(!is_tag_implicit){
	    if((type_field(node->type)==TYPE_SEQUENCE) ||
	       (type_field(node->type)==TYPE_SEQUENCE_OF) ||
	       (type_field(node->type)==TYPE_SET) ||
	       (type_field(node->type)==TYPE_SET_OF))  class2|=STRUCTURED;
	    class_implicit=class2;
	    tag_implicit=strtoul(p->value,NULL,10);
	    is_tag_implicit=1;
	  }
	}
      }
      p=p->right;
    }
  }

  if(is_tag_implicit){
    tag=_asn1_get_tag_der(der+counter,&class,&len2);
    if((class!=class_implicit) || (tag!=tag_implicit)) return ASN1_TAG_ERROR;
  }
  else{
    if(type_field(node->type)==TYPE_TAG){
      counter=0;
      *der_len=counter;
      return ASN1_SUCCESS;
    }

    tag=_asn1_get_tag_der(der+counter,&class,&len2);
    switch(type_field(node->type)){
    case TYPE_NULL:
      if((class!=UNIVERSAL) || (tag!=TAG_NULL)) return ASN1_DER_ERROR;
       break;
    case TYPE_BOOLEAN:
      if((class!=UNIVERSAL) || (tag!=TAG_BOOLEAN)) return ASN1_DER_ERROR;
       break;
    case TYPE_INTEGER:
      if((class!=UNIVERSAL) || (tag!=TAG_INTEGER)) return ASN1_DER_ERROR;
       break;
    case TYPE_ENUMERATED:
      if((class!=UNIVERSAL) || (tag!=TAG_ENUMERATED)) return ASN1_DER_ERROR;
       break;
    case TYPE_OBJECT_ID:
      if((class!=UNIVERSAL) || (tag!=TAG_OBJECT_ID)) return ASN1_DER_ERROR;
       break;
    case TYPE_TIME:
      if(node->type&CONST_UTC){
	  if((class!=UNIVERSAL) || (tag!=TAG_UTCTime)) return ASN1_DER_ERROR;
      }
      else{
	if((class!=UNIVERSAL) || (tag!=TAG_GENERALIZEDTime)) 
	  return ASN1_DER_ERROR;
      }
      break;
    case TYPE_OCTET_STRING:
      if((class!=UNIVERSAL) || (tag!=TAG_OCTET_STRING)) return ASN1_DER_ERROR;
      break;
    case TYPE_BIT_STRING:
      if((class!=UNIVERSAL) || (tag!=TAG_BIT_STRING)) return ASN1_DER_ERROR;
      break;
    case TYPE_SEQUENCE: case TYPE_SEQUENCE_OF:
      if((class!=(UNIVERSAL|STRUCTURED)) || (tag!=TAG_SEQUENCE)) 
	return ASN1_DER_ERROR;
      break;
    case TYPE_SET: case TYPE_SET_OF:
      if((class!=(UNIVERSAL|STRUCTURED)) || (tag!=TAG_SET)) 
	return ASN1_DER_ERROR;
      break;
    case TYPE_ANY:
      counter-=len2;
      break;
    default:
      return ASN1_DER_ERROR;
      break;
    }
  }

  counter+=len2;
  *der_len=counter;
  return ASN1_SUCCESS;
}


int 
_asn1_delete_not_used(node_asn *node)
{
  node_asn *p,*p2;

  if(node==NULL) return ASN1_ELEMENT_NOT_FOUND;

  p=node;
  while(p){
    if(p->type&CONST_NOT_USED){
      p2=NULL;
      if(p!=node){
	p2=_asn1_find_left(p);
	if(!p2) p2=_asn1_find_up(p);
      }
      asn1_delete_structure(&p);
      p=p2;
    } 

    if(!p) break;  /* reach node */

    if(p->down){
      p=p->down;
    }
    else{
      if(p==node) p=NULL;
      else if(p->right) p=p->right;
      else{
	while(1){
	  p=_asn1_find_up(p);
	  if(p==node){
	    p=NULL;
	    break;
	  }
	  if(p->right){
	    p=p->right;
	    break;
	  }
	}
      }
    }
  }
  return ASN1_SUCCESS;
}



/**
  * asn1_der_decoding - Fill the structure *ELEMENT with values of a DER encoding string.
  * @element: pointer to an ASN1 structure
  * @der: vector that contains the DER encoding. 
  * @len: number of bytes of *der: der[0]..der[len-1]
  * Description:
  *
  * Fill the structure *ELEMENT with values of a DER encoding string. The sructure must just be
  * created with function 'create_stucture'.
  * If an error accurs during de decoding procedure, the *ELEMENT is deleted
  * and set equal to ASN1_TYPE_EMPTY.
  *
  * Returns:
  *
  *   ASN1_SUCCESS\: DER encoding OK
  *
  *   ASN1_ELEMENT_NOT_FOUND\: ELEMENT is ASN1_TYPE_EMPTY.
  *
  *   ASN1_TAG_ERROR,ASN1_DER_ERROR\: the der encoding doesn't match the structure NAME. *ELEMENT deleted. 
  **/

asn1_retCode
asn1_der_decoding(ASN1_TYPE *element,unsigned char *der,int len,char *errorDescription)
{
  node_asn *node,*p,*p2,*p3;
  char temp[128];
  int counter,len2,len3,len4,move,ris;
  unsigned char class,*temp2;
  unsigned int tag;

  node=*element;

  if(node==ASN1_TYPE_EMPTY) return ASN1_ELEMENT_NOT_FOUND;

  if(node->type&CONST_OPTION){
    asn1_delete_structure(element);
    return ASN1_GENERIC_ERROR;
  }

  counter=0;
  move=DOWN;
  p=node;
  while(1){
    ris=ASN1_SUCCESS;

    if(move!=UP){
      if(p->type&CONST_SET){
	p2=_asn1_find_up(p);
	len2=strtol(p2->value,NULL,10);
	if(counter==len2){
	  p=p2;
	  move=UP;
	  continue;
	}
	else if(counter>len2){
	  asn1_delete_structure(element);
	  return ASN1_DER_ERROR;
	}
	p2=p2->down;
	while(p2){
	  if((p2->type&CONST_SET) && (p2->type&CONST_NOT_USED)){
	    if(type_field(p2->type)!=TYPE_CHOICE)
	      ris=_asn1_extract_tag_der(p2,der+counter,&len2);
	    else{
	      p3=p2->down;
	      while(p3){
		ris=_asn1_extract_tag_der(p3,der+counter,&len2);
		if(ris==ASN1_SUCCESS) break;
		//if(ris==ASN1_ERROR_TYPE_ANY) return ASN1_ERROR_TYPE_ANY;
		p3=p3->right;
	      }
	    }
	    if(ris==ASN1_SUCCESS){
	      p2->type&=~CONST_NOT_USED;
	      p=p2;
	      break;
	    }
	    //else if(ris==ASN1_ERROR_TYPE_ANY) return ASN1_ERROR_TYPE_ANY;
	  }
	  p2=p2->right;
	}
	if(p2==NULL){
	  asn1_delete_structure(element);
	  return ASN1_DER_ERROR;
	}
      }

      if(type_field(p->type)==TYPE_CHOICE){
	while(p->down){
	  ris=_asn1_extract_tag_der(p->down,der+counter,&len2);
	  if(ris==ASN1_SUCCESS){
	    while(p->down->right){
	      p2=p->down->right;
	      asn1_delete_structure(&p2);
	    }
	    break;
	  }
	  else if(ris==ASN1_ERROR_TYPE_ANY){
	    asn1_delete_structure(element);
	    return ASN1_ERROR_TYPE_ANY;
	  }
	  else{
	    p2=p->down;
	    asn1_delete_structure(&p2);
	  }
	}
	if(p->down==NULL){
	  asn1_delete_structure(element);
	  return ASN1_DER_ERROR;
	}
	p=p->down;
      }

      if((p->type&CONST_OPTION) || (p->type&CONST_DEFAULT)){
	p2=_asn1_find_up(p);
	len2=strtol(p2->value,NULL,10);
	if(counter>=len2) ris=ASN1_TAG_ERROR;
      }

      if(ris==ASN1_SUCCESS) ris=_asn1_extract_tag_der(p,der+counter,&len2);
      if(ris!=ASN1_SUCCESS){
	//if(ris==ASN1_ERROR_TYPE_ANY) return ASN1_ERROR_TYPE_ANY;
	if(p->type&CONST_OPTION){
	  p->type|=CONST_NOT_USED;
	  move=RIGHT;
	}
	else if(p->type&CONST_DEFAULT) {
	  _asn1_set_value(p,NULL,0);
	  move=RIGHT;
	}
	else {
	  //return (type_field(p->type)!=TYPE_ANY)?ASN1_TAG_ERROR:ASN1_ERROR_TYPE_ANY;
	  _asn1_error_description_tag_error(p,errorDescription);
	  
	  asn1_delete_structure(element);
	  return ASN1_TAG_ERROR;
	}
      } 
      else counter+=len2;
    }

    if(ris==ASN1_SUCCESS){
      switch(type_field(p->type)){
      case TYPE_NULL:
	if(der[counter]){
	  asn1_delete_structure(element);
	  return ASN1_DER_ERROR;
	}
	counter++;
	move=RIGHT;
	break;
      case TYPE_BOOLEAN:
	if(der[counter++]!=1){
	  asn1_delete_structure(element);
	  return ASN1_DER_ERROR;
	}
	if(der[counter++]==0) _asn1_set_value(p,"F",1);
	else _asn1_set_value(p,"T",1);
	move=RIGHT;
	break;
      case TYPE_INTEGER: case TYPE_ENUMERATED:
	len2=_asn1_get_length_der(der+counter,&len3);
	_asn1_set_value(p,der+counter,len3+len2);
	counter+=len3+len2;
	move=RIGHT;
	break;
      case TYPE_OBJECT_ID:
	_asn1_get_objectid_der(der+counter,&len2, temp, sizeof(temp));
	_asn1_set_value(p,temp,strlen(temp)+1);
	counter+=len2;
	move=RIGHT;
      break;
      case TYPE_TIME:
	_asn1_get_time_der(der+counter,&len2,temp);
	_asn1_set_value(p,temp,strlen(temp)+1);
	counter+=len2;
	move=RIGHT;
	break;
      case TYPE_OCTET_STRING:
	len2=_asn1_get_length_der(der+counter,&len3);
	_asn1_set_value(p,der+counter,len3+len2);
	counter+=len3+len2;
	move=RIGHT;
	break;
      case TYPE_BIT_STRING:
	len2=_asn1_get_length_der(der+counter,&len3);
	_asn1_set_value(p,der+counter,len3+len2);
	counter+=len3+len2;
	move=RIGHT;
	break;
      case TYPE_SEQUENCE:  case TYPE_SET:;
	if(move==UP){
	  len2=strtol(p->value,NULL,10);
	  _asn1_set_value(p,NULL,0);
	  if(len2!=counter){
	    asn1_delete_structure(element);
	    return ASN1_DER_ERROR;
	  }
	  move=RIGHT;
	}
	else{   /* move==DOWN || move==RIGHT */
	  len3=_asn1_get_length_der(der+counter,&len2);
	  counter+=len2;
	  _asn1_ltostr(counter+len3,temp);
	  _asn1_set_value(p,temp,strlen(temp)+1);
	  move=DOWN; 
	}
	break;
      case TYPE_SEQUENCE_OF: case TYPE_SET_OF:
	if(move==UP){
	  len2=strtol(p->value,NULL,10);
	  if(len2>counter){
	    _asn1_append_sequence_set(p);
	    p=p->down;
	    while(p->right) p=p->right;
	    move=RIGHT;
	    continue;
	  }
	  _asn1_set_value(p,NULL,0);
	  if(len2!=counter){
	    asn1_delete_structure(element);
	    return ASN1_DER_ERROR;
	  }
	}
	else{   /* move==DOWN || move==RIGHT */
	  len3=_asn1_get_length_der(der+counter,&len2);
	  counter+=len2;
	  if(len3){
	    _asn1_ltostr(counter+len3,temp);
	    _asn1_set_value(p,temp,strlen(temp)+1);
	    p2=p->down;
	    while((type_field(p2->type)==TYPE_TAG) || (type_field(p2->type)==TYPE_SIZE)) p2=p2->right;
	    if(p2->right==NULL) _asn1_append_sequence_set(p);
	    p=p2;
	  }
	}
	move=RIGHT;
	break;
      case TYPE_ANY:
	tag=_asn1_get_tag_der(der+counter,&class,&len2);
	len2+=_asn1_get_length_der(der+counter+len2,&len3);
	_asn1_length_der(len2+len3,NULL,&len4);
	temp2=(unsigned char *)_asn1_alloca(len2+len3+len4);
        if (temp2==NULL){
	  asn1_delete_structure(element);
	  return ASN1_MEM_ERROR;
	}
        
	_asn1_octet_der(der+counter,len2+len3,temp2,&len4);
	_asn1_set_value(p,temp2,len4);
	_asn1_afree(temp2);
	counter+=len2+len3;
	move=RIGHT;
	break;
      default:
	move=(move==UP)?RIGHT:DOWN;
	break;
      }
    }

    if(p==node && move!=DOWN) break;

    if(move==DOWN){
      if(p->down) p=p->down;
      else move=RIGHT;
    }
    if((move==RIGHT) && !(p->type&CONST_SET)){
      if(p->right) p=p->right;
      else move=UP;
    }
    if(move==UP) p=_asn1_find_up(p);
  }

  _asn1_delete_not_used(*element);

  if(counter != len){
    asn1_delete_structure(element);
    return ASN1_DER_ERROR;
  }

  return ASN1_SUCCESS;
}


/**
  * asn1_der_decoding_startEnd - Find the start and end point of an element in a DER encoding string.
  * @element: pointer to an ASN1 element
  * @der: vector that contains the DER encoding. 
  * @len: number of bytes of *der: der[0]..der[len-1]
  * @name_element: an element of NAME structure.
  * @start: the position of the first byte of NAME_ELEMENT decoding (der[*start]) 
  * @end: the position of the last byte of NAME_ELEMENT decoding (der[*end])
  * Description:
  * 
  * Find the start and end point of an element in a DER encoding string. I mean that if you
  * have a der encoding and you have already used the function "asn1_der_decoding" to fill a structure, it may
  * happen that you want to find the piece of string concerning an element of the structure.
  * 
  * Example: the sequence "tbsCertificate" inside an X509 certificate.
  *
  * Returns:
  *
  *   ASN1_SUCCESS\: DER encoding OK
  *
  *   ASN1_ELEMENT_NOT_FOUND\: ELEMENT is ASN1_TYPE EMPTY  or NAME_ELEMENT is not a valid element.
  *
  *   ASN1_TAG_ERROR,ASN1_DER_ERROR\: the der encoding doesn't match the structure ELEMENT.
  *
  **/
asn1_retCode
asn1_der_decoding_startEnd(ASN1_TYPE element,unsigned char *der,int len,char *name_element,int *start, int *end)
{
  node_asn *node,*node_to_find,*p,*p2,*p3;
  int counter,len2,len3,move,ris;
  unsigned char class;
  unsigned int tag;

  node=element;

  if(node==ASN1_TYPE_EMPTY) return ASN1_ELEMENT_NOT_FOUND;

  node_to_find=_asn1_find_node(node,name_element);

  if(node_to_find==NULL) return ASN1_ELEMENT_NOT_FOUND;

  if(node_to_find==node){
    *start=0;
    *end=len-1;
    return ASN1_SUCCESS;
  }

  if(node->type&CONST_OPTION) return ASN1_GENERIC_ERROR;

  counter=0;
  move=DOWN;
  p=node;
  while(1){
    ris=ASN1_SUCCESS;
    
    if(move!=UP){
      if(p->type&CONST_SET){
	p2=_asn1_find_up(p);
	len2=strtol(p2->value,NULL,10);
	if(counter==len2){
	  p=p2;
	  move=UP;
	  continue;
	}
	else if(counter>len2) return ASN1_DER_ERROR;
	p2=p2->down;
	while(p2){
	  if((p2->type&CONST_SET) && (p2->type&CONST_NOT_USED)){  /* CONTROLLARE */
	    if(type_field(p2->type)!=TYPE_CHOICE)
	      ris=_asn1_extract_tag_der(p2,der+counter,&len2);
	    else{
	      p3=p2->down;
	      ris=_asn1_extract_tag_der(p3,der+counter,&len2);
	    }
	    if(ris==ASN1_SUCCESS){
	      p2->type&=~CONST_NOT_USED;
	      p=p2;
	      break;
	    }
	  }
	  p2=p2->right;
	}
	if(p2==NULL) return ASN1_DER_ERROR;
      }

      if(p==node_to_find) *start=counter;

      if(type_field(p->type)==TYPE_CHOICE){
	p=p->down;
	ris=_asn1_extract_tag_der(p,der+counter,&len2);
	if(p==node_to_find) *start=counter;
      }

      if(ris==ASN1_SUCCESS) ris=_asn1_extract_tag_der(p,der+counter,&len2);
      if(ris!=ASN1_SUCCESS){
	if(p->type&CONST_OPTION){
	  p->type|=CONST_NOT_USED;
	  move=RIGHT;
	}
	else if(p->type&CONST_DEFAULT) {
	  move=RIGHT;
	}
	else {
	  return ASN1_TAG_ERROR;
	}
      } 
      else counter+=len2;
    }

    if(ris==ASN1_SUCCESS){
      switch(type_field(p->type)){
      case TYPE_NULL:
     	if(der[counter]) return ASN1_DER_ERROR;
	counter++;
	move=RIGHT;
	break;
      case TYPE_BOOLEAN:
	if(der[counter++]!=1) return ASN1_DER_ERROR;
	counter++;
	move=RIGHT;
	break;
      case TYPE_INTEGER: case TYPE_ENUMERATED:
	len2=_asn1_get_length_der(der+counter,&len3);
	counter+=len3+len2;
	move=RIGHT;
	break;
      case TYPE_OBJECT_ID:
	len2=_asn1_get_length_der(der+counter,&len3);
	counter+=len2+len3;
	move=RIGHT;
      break;
      case TYPE_TIME:
	len2=_asn1_get_length_der(der+counter,&len3);
	counter+=len2+len3;
	move=RIGHT;
	break;
      case TYPE_OCTET_STRING:
	len2=_asn1_get_length_der(der+counter,&len3);
	counter+=len3+len2;
	move=RIGHT;
	break;
      case TYPE_BIT_STRING:
	len2=_asn1_get_length_der(der+counter,&len3);
	counter+=len3+len2;
	move=RIGHT;
	break;
      case TYPE_SEQUENCE:  case TYPE_SET:
	if(move!=UP){
	  len3=_asn1_get_length_der(der+counter,&len2);
	  counter+=len2;
	  move=DOWN; 
	}
	else move=RIGHT;
	break;
      case TYPE_SEQUENCE_OF: case TYPE_SET_OF:
	if(move!=UP){
	  len3=_asn1_get_length_der(der+counter,&len2);
	  counter+=len2;
	  if(len3){
	    p2=p->down;
	    while((type_field(p2->type)==TYPE_TAG) || 
		  (type_field(p2->type)==TYPE_SIZE)) p2=p2->right;
	    p=p2;
	  }
	}
	move=RIGHT;
	break;
      case TYPE_ANY:
	tag=_asn1_get_tag_der(der+counter,&class,&len2);
	len2+=_asn1_get_length_der(der+counter+len2,&len3);
	counter+=len3+len2;
 	move=RIGHT;
	break;
      default:
	move=(move==UP)?RIGHT:DOWN;
	break;
      }
    }

    if((p==node_to_find) && (move==RIGHT)){
      *end=counter-1;
      return ASN1_SUCCESS;
    }

    if(p==node && move!=DOWN) break;

    if(move==DOWN){
      if(p->down) p=p->down;
      else move=RIGHT;
    }
    if((move==RIGHT) && !(p->type&CONST_SET)){
      if(p->right) p=p->right;
      else move=UP;
    }
    if(move==UP) p=_asn1_find_up(p);
  }

  return ASN1_ELEMENT_NOT_FOUND;
}










