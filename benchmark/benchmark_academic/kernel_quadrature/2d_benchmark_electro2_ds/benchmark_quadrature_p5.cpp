#include "helper.h"
#include "scalar_impl.h"


// compile time constants
constexpr int ndim          = 2;
constexpr int nnode         = 3578; 
constexpr int nelem         = 273;
constexpr int nodeperelem   = 21;
constexpr int ngauss        = 19;

///
template<typename Ti, typename Tf, size_t ndim, size_t nelem, size_t nnode, size_t nodeperelem, size_t ngauss>
void run(const Tensor<Ti,nelem,nodeperelem> &elements, const Tensor<Tf,nnode,ndim> &points,
        const Tensor<Tf,ndim,nodeperelem,ngauss> &Jm, const Tensor<Tf,ngauss,1> &AllGauss) {

    volatile Tf u1 = random()+2;
    volatile Tf u2 = random()+1;
    volatile Tf ue = random()+0.05;
    volatile Tf e1 = random()+2;
    volatile Tf e2 = random()+1;
    volatile Tf ee = random()+0.05;
    volatile Tf kappa = random()+3;
    auto I = eye<Tf,ndim,ndim,ndim,ndim>();
    
    Tf *Jm_data = Jm.data();

    for (int elem=0; elem<nelem; ++elem) {
        Tensor<Tf,nodeperelem,ndim> LagrangeElemCoords;
        Tensor<Tf,nodeperelem,ndim> EulerElemCoords;
        Tensor<Tf,ndim> D0; D0.random();


        for (int i=0; i<nodeperelem; ++i) {
            LagrangeElemCoords(i,0) = points(elements(elem,i),0);
            EulerElemCoords(i,0) = points(elements(elem,i),0);
            LagrangeElemCoords(i,1) = points(elements(elem,i),1);
            EulerElemCoords(i,1) = points(elements(elem,i),1);
        }

        for (int g=0; g<ngauss; ++g) {
            // Get Gauss point Jm
            constexpr int lsize = ndim*nodeperelem;
            Tensor<Tf,ndim,nodeperelem> Jm_g;
            Tf *Jm_g_data = Jm_g.data();
            for (int i=0; i<lsize; i+=2) {
                Jm_g_data[i] = Jm_data[i+ngauss*g];
                Jm_g_data[i+1] = Jm_data[i+1+ngauss*g];
            }

            // Compute gradient of shape functions
            auto ParentGradientX = matmul(Jm_g,LagrangeElemCoords);
            // Compute material gradient
            auto MaterialGradient = matmul(inverse(ParentGradientX),Jm_g);

            // Compute the deformation gradient tensor
            auto F = matmul(MaterialGradient,EulerElemCoords);
            // Compute H
            auto H = cofactor(F);
            // auto H = sv::cofactor(F);
            // Compute J
            auto J = determinant(F);
            // Compute d=FD0
            auto d = matmul(F,D0);

            // Compute work-conjugates
            auto WF = 2.*u1*F + 4*ue*dot(F,F)*F + 2/ee*dot(d,d);
            auto WH = 2.*u2*H;
            auto WJ = -(2*u1+4*u2+12*ue)/J+kappa*(J-1) - (1./2./e2/J/J)*dot(d,d);
            auto WD0 = 1/e1*D0;
            auto Wd = 1/e2/J*d + 2/ee*dot(F,F)*d + 4/ue/ee/ee*dot(d,d)*d;
            // Compute first Piola-Kirchhoff stress tensor
            auto P = WF + cross<PlaneStrain>(static_cast<Tensor<Tf,ndim,ndim>>(WH),F) + WJ*H;
            // Compute electric field
            auto E0 = WD0 + matmul(transpose(F),static_cast<Tensor<Tf,ndim>>(Wd));


            // Compute Hessian components
            auto WFF = 2.*u1*I + 4*ue*dot(F,F)*I+8*ue*outer(F,F);
            auto WFd = 4/ee*d;
            auto WHH = 2.*u2*I;
            auto WJJ = 1./J/J*(2*u1+4*u2+12*ue)+kappa + 1/4/e2/J/J/J*dot(d,d);
            auto WJd = -1/e2/J/J*d;
            auto WD0D0 = 1/e1*eye2<Tf,ndim,ndim>();
            auto Wdd = 1/e2/J*eye2<Tf,ndim,ndim>() + 2/ee*dot(F,F)*eye2<Tf,ndim,ndim>()+
                4/ue/ee/ee*dot(d,d)*eye2<Tf,ndim,ndim>() + 8/ue/ee/ee*outer(d,d);

            // dump
            unused(P,E0);
            unused(WFF,WFd,WHH,WJJ,WJd,WD0D0,Wdd);
        }
    }
}


int main() {

    std::string efile = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/mesh_2d_elements_p5.dat";
    std::string pfile = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/mesh_2d_points_p5.dat";
    std::string gfile0 = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/p5_2d_Jm.dat";
    std::string gfile1 = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/p5_2d_AllGauss.dat";


    Tensor<size_t,nelem,nodeperelem> elements = loadtxt<size_t,nelem,nodeperelem>(efile);
    Tensor<real,nnode,ndim> points = loadtxt<real,nnode,ndim>(pfile);
    // Fill with uniformly distributed random values
    decltype(points) Eulerx; Eulerx.random();

    Tensor<real,ndim*nodeperelem*ngauss,1> Jm_temp = loadtxt<real,ndim*nodeperelem*ngauss,1>(gfile0);
    Tensor<real,ndim,nodeperelem,ngauss> Jm;
    std::copy(Jm_temp.data(),Jm_temp.data()+Jm_temp.Size,Jm.data()); 
    Tensor<real,ngauss,1> AllGauss = loadtxt<real,ngauss,1>(gfile1);

    // check call
    run(elements,points,Jm,AllGauss);
    // run benchmark
    timeit(static_cast<void (*)(const Tensor<size_t,nelem,nodeperelem> &, const Tensor<real,nnode,ndim> &,
        const Tensor<real,ndim,nodeperelem,ngauss> &, const Tensor<real,ngauss,1> &)>(&run),elements,points,Jm,AllGauss);

    return 0;
}