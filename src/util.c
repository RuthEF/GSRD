
#include "util.h"

#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))


// Replacement for strchr() (not correctly supported by pgi-cc) with extras
const char *sc (const char *s, const char c, const char * const e, const I8 o)
{
   if (s)
   {
      if (e) { while ((s <= e) && (*s != 0) && (*s != c)) { ++s; } }
      else { while ((*s != 0) && (*s != c)) { ++s; } }
      if (*s == c) { return(s+o); }
   }
   return(NULL);
} // sc

const char *stripPath (const char *path)
{
   if (path && *path)
   {
      const char *t= path, *l= path;
      do
      {
         path= t;
         while (('\\' == *t) || ('/' == *t) || (':' == *t)) { ++t; }
         if (t > path) { l= t; }
         t += (0 != *t);
      } while (*t);
      return(l);
   }
   return(NULL);
} // stripPath

// const char *extractPath (char s[], int m, const char *pfe) {} // extractPath

size_t fileSize (const char * const path)
{
   if (path && path[0])
   {
      struct stat sb={0};
      int r= stat(path, &sb);
      if (0 == r) { return(sb.st_size); }
   }
   return(0);
} // fileSize

size_t loadBuff (void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"r");
   if (hF)
   {
      size_t r= fread(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // loadBuff

size_t saveBuff (const void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"w");
   if (hF)
   {
      size_t r= fwrite(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // saveBuff

SMVal deltaT (void)
{
   static struct timeval t[2]={0,};
   static int i= 0;
   SMVal dt;
   GETTIME(t+i);
   dt= 1E-6 * USEC(t[i^1],t[i]);
   i^= 1;
   return(dt);
} // deltaT

U32 statGetRes1 (StatRes1 * const pR, const StatMom * const pS, const SMVal dof)
{
   StatRes1 r={ 0, 0 };
   U32 o= 0;
   if (pS && (pS->m[0] > 0))
   {
      r.m= pS->m[1] / pS->m[0];
      ++o;
      if (pS->m[0] != dof)
      { 
         //SMVal ess= (pS->m[1] * pS->m[1]) / pS->m[0];
         r.v= ( pS->m[2] - (pS->m[1] * r.m) ) / (pS->m[0] - dof);
         ++o;
      }
   }
   if (pR) { *pR= r; }
   return(o);
} // statGetRes1

int scanZD (size_t * const pZ, const char s[])
{
   const char *pE=NULL;
   int n=0;
   long long int z= strtoll(s, (char**)&pE, 10);
   if (pE && (pE > s))
   {
      if (pZ) { *pZ= z; }
      n= pE - s;
   }
   return(n);
} // scanZD

int scanRevZD (size_t * const pZ, const char s[], const int e)
{
   int i= e;
   while ((i >= 0) && isdigit(s[i])) { --i; }
   i+= !isdigit(s[i]);
   //printf("scanRevZD() [%d]=%s\n", i, s+i);
   if (i <= e) { return scanZD(pZ, s+i); }
   //else
   return(0);
} // scanRevZD

int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[])
{
   ScanSeg ss={0,0};
   const char *pT1, *pT2;
   int nI=0;
   
   pT1= sc(s, '(', NULL, 0);
   pT2= sc(pT1, ')', NULL, 0);
   if (pT1 && pT2 && isdigit(pT1[1]) && isdigit(pT2[-1]))
   {  // include delimiters in segment...
      ss.start= pT1 - s;
      ss.len= pT2 + 1 - pT1;
      // but exclude from further scan
      ++pT1; --pT2;
      do
      {
         const char *pE=NULL;
         v[nI]= strtol(pT1, (char**)&pE, 10);
         if (pE && (pE > pT1)) { nI++; pT1= pE; }
         //v[nI++]= atoi(pT1);
         pT1= sc(pT1, ',', pT2, 1);
      } while (pT1 && (nI < vMax));
   }
   if (pSS) { *pSS= ss; }
   return(nI);
} // scanVI

U8 scanChZ (const char *s, char c)
{
   if (s)
   {
      c= toupper(c);
      while (*s && (toupper(*s) != c)) { ++s; }
      if (*s)
      {
         size_t z;
         if ((scanZD(&z, s+1) > 0) && (z > 0) && (z <= 64)) { return((U8)z); }
      }
   }
   return(0);
} // scanChZ

size_t scanDFI (DataFileInfo * pDFI, const char * const path)
{
   size_t bytes= fileSize(path);
   if (bytes > 0)
   {
      const char *pCh, *name=  stripPath(path);
      pDFI->filePath=   path;

      pDFI->bytes= bytes;
      pDFI->nV=    scanVI(pDFI->v, 4, &(pDFI->vSS), name);
      pCh= name + pDFI->vSS.start + pDFI->vSS.len;
      pDFI->elemBits= scanChZ(pCh, 'F');
      scanRevZD(&(pDFI->iter), name, pDFI->vSS.start-1);
      if (pDFI->nV > 0)
      {
         size_t bits= pDFI->elemBits;
         printf("%s -> v[%d]=(", name, pDFI->nV);
         for (int i=0; i < pDFI->nV; i++)
         {
            bits*= pDFI->v[i];
            printf("%d,", pDFI->v[i]);
         }
         printf(")*%u ", pDFI->elemBits);
         if (pDFI->bytes == ((bits+7) >> 3)) { pDFI->flags|= DFI_FLAG_VALIDATED; printf("OK\n"); }
         if (0 == (pDFI->flags & DFI_FLAG_VALIDATED)) { printf("WARNING: scanDFI() %zuBytes in file, expected %zubits\n", pDFI->bytes, bits); }
      }
   }
   return(bytes);
} // scanDFI

int contigCharSetN (const char s[], const int maxS, const char t[], const int maxT)
{
   int n= 0;
   while ((n < maxS) && s[n])
   {
      int i= 0;
      while ((i < maxT) && t[i] && (s[n] != t[i])) { ++i; }
      if (s[n] != t[i]) { return(n); }
      // else
      ++n;
   }
   return(n);
} // contigCharSetN

int scanArgs (ArgInfo *pAI, const char * const a[], int nA)
{
   const char *pCh;
   ArgInfo tmpAI;
   int nV= 0;
   
   if (NULL == pAI) { pAI= &tmpAI; }
   while (nA-- > 0)
   {
      pCh= a[nA];
      if ('-' != pCh[0]) { nV+= ( scanDFI(&(pAI->files.init), pCh) > 0 ); }
      else
      {
         char c, v=0;
         int n= contigCharSetN(pCh+0, 2, "-", 2);
         c= pCh[n++];
         n+= contigCharSetN(pCh+n, 2, ":=", 2);
         switch(toupper(c))
         {
            case 'A' :
               v= toupper( pCh[n] );
               if ('H' == v) { pAI->proc.flags|= PROC_FLAG_ACCHOST; }
               if ('G' == v) { pAI->proc.flags|= PROC_FLAG_ACCGPU; }
               if ('A' == v) { pAI->proc.flags|= PROC_FLAG_ACCHOST|PROC_FLAG_ACCGPU; }
               if ('N' == v) { pAI->proc.flags&= ~(PROC_FLAG_ACCHOST|PROC_FLAG_ACCGPU); }
               ++nV;
               break;

            case 'C' : nV+= ( scanDFI(&(pAI->files.cmp), pCh+n) > 0 );
               //printf("cmp:iter=%zu\n", pAI->files.cmp.iter);
               break;

            case 'I' :
               n+= scanZD(&(pAI->proc.maxIter), pCh+n);
               n+= contigCharSetN(pCh+n, 2, ",;:", 3);
               n+= scanZD(&(pAI->proc.subIter), pCh+n);
               ++nV;
               break;
          
            case 'O' : pAI->files.outPath= pCh+n;
               break;

            default : printf("scanArgs() - unknown flag -%c\n", c);
         }
      }
   }
   //if (0 == pAI->proc.flags) { pAI->proc.flags= PROC_FLAG_HOST|PROC_FLAG_GPU; }
   if (0 == pAI->proc.maxIter) { pAI->proc.maxIter= 5000; }
   if (0 == pAI->proc.subIter) { pAI->proc.subIter= 1000; }
   if (pAI->proc.subIter > pAI->proc.maxIter) SWAP(size_t, pAI->proc.subIter, pAI->proc.maxIter);
   return(nV);
} // scanArgs

#ifdef UTIL_TEST

int utilTest (void)
{
   return(0);
} // utilTest

#endif

#ifdef UTIL_MAIN

int main (int argc, char *argv[])
{
   return utilTest();
} // utilTest

#endif
