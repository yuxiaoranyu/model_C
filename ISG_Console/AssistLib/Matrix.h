#ifndef MATRIX_H
#define MATRIX_H
/* Matrix.h -- define types for matrices using Iliffe vectors
 *
 *************************************************************
 * HISTORY
 *
 * 02-Apr-95  Reg Willson (rgwillson@mmm.com) at 3M St. Paul, MN
 *      Rewrite memory allocation to avoid memory alignment problems
 *      on some machines.
 */

typedef struct
{
    int       lb1, //行开始索引
              ub1, //行结束索引
              lb2, //列开始索引
              ub2; //列结束索引
    char     *mat_sto; //矩阵内存的起始地址
    double  **el; //矩阵内容每一行，列索引为0的内存地址
} dmat;

void    print_mat (); //打印矩阵
dmat    newdmat (int,int,int,int,int*); //为矩阵分配和初始化内存
int     matmul (); //矩阵相乘
int     matcopy (); //矩阵复制
int     transpose (); //矩阵转置
double  matinvert (); //矩阵原地旋转
int     solveSystem(dmat,dmat,dmat);
bool    solveTransMatrix(float *xs, float *ys, float *xd, float *yd, int count, float *para, float *meanErr);

#define freemat(m) free((m).mat_sto) ; free((m).el)



#endif // MATRIX_H
