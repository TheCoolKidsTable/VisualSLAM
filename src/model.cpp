
#include "model.hpp"
#include <iostream>

#include<opencv2/opencv.hpp>

using std::sqrt;

void SlamProcessModel::operator()(const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd & f)
{
    // TODO: mean function, x = [nu;eta;landmarks]
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> J(6,6);
    J.setZero();

    // Rnc
    Eigen::Matrix<double, Eigen::Dynamic, 1> Thetanc(3,1);
    Thetanc = x.block(9,0,3,1);
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> Rnc(3,3);
    rpy2rot(Thetanc,Rnc);

    // Kinematic transform T(nu)
    double phi   = Thetanc(0);
    double theta = Thetanc(1);
    double psi   = Thetanc(2);

    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> T(3,3);
    using std::tan;
    using std::cos;
    using std::sin;
    T << 1, sin(phi)*tan(theta),cos(phi)*tan(theta),
        0,cos(phi),-sin(phi),
        0,sin(phi)/cos(theta),cos(phi)/cos(theta);

    J.block(0,0,3,3) = Rnc;
    J.block(3,3,3,3) = T;

    f.resize(x.rows(),1);
    f.setZero();
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> nu(6,1);
    nu = x.block(0,0,6,1);
    f.block(6,0,6,1) = J*nu;

}

void SlamProcessModel::operator()(const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd & f, Eigen::MatrixXd & SQ)
{
    operator()(x,u,param,f);
    SQ.resize(x.rows(),x.rows());
    SQ.setZero();
    Eigen::MatrixXd position_tune = param.position_tune;
    Eigen::MatrixXd orientation_tune = param.orientation_tune;
    SQ.block(0,0,3,3) = position_tune;
    SQ.block(3,3,3,3) = orientation_tune;
}


void SlamProcessModel::operator()(const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd & f, Eigen::MatrixXd & SQ, Eigen::MatrixXd &dfdx)
{
    int nx = x.rows();
    int n_landmark = param.n_landmark;
    int nj = (nx - 12)/n_landmark;
    operator()(x,u,param,f,SQ);
    dfdx.resize(nx,12+n_landmark*nj);
    dfdx.setZero();

    Eigen::MatrixXd J(6,6);
    J.setZero();

    Eigen::MatrixXd eta(6,1);
    eta = x.block(6,0,6,1);
    Eigen::MatrixXd nu(6,1);
    nu  = x.block(0,0,6,1);


    double x1       = eta(0);
    double x2       = eta(1);
    double x3       = eta(2);
    double phi      = eta(3);
    double theta    = eta(4);
    double psi      = eta(5);

    double x1dot    = nu(0);
    double x2dot    = nu(1);
    double x3dot    = nu(2);
    double phidot   = nu(3);
    double thetadot = nu(4);
    double psidot   = nu(5);

    // Rnc
    Eigen::Matrix<double, Eigen::Dynamic, 1> Thetanc(3,1);
    Thetanc = x.block(9,0,3,1);
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> Rnc(3,3);
    rpy2rot(Thetanc,Rnc);

    Eigen::MatrixXd T(3,3);
    using std::tan;
    using std::cos;
    using std::sin;

    T << 1, sin(phi)*tan(theta),cos(phi)*tan(theta),
        0,cos(phi),-sin(phi),
        0,sin(phi)/cos(theta),cos(phi)/cos(theta);

    J.block(0,0,3,3) = Rnc;
    J.block(3,3,3,3) = T;

    dfdx.block(6,0,6,6) = J;

    Eigen::MatrixXd dfdnu(6,6);
    dfdnu.setZero();

    using std::sin;
    using std::cos;
    using std::tan;
    using std::pow;

    dfdnu(0,3) = x2dot*(sin(phi)*sin(psi) + cos(phi)*cos(psi)*sin(theta)) + x3dot*(cos(phi)*sin(psi) - cos(psi)*sin(phi)*sin(theta));
    dfdnu(1,3) = -x2dot*(cos(psi)*sin(phi) - cos(phi)*sin(psi)*sin(theta)) - x3dot*(cos(phi)*cos(psi) + sin(phi)*sin(psi)*sin(theta));
    dfdnu(2,3) = x2dot*cos(phi)*cos(theta) - x3dot*cos(theta)*sin(phi);
    dfdnu(3,3) = thetadot*cos(phi)*tan(theta) - psidot*sin(phi)*tan(theta);
    dfdnu(4,3) = -psidot*cos(phi) - thetadot*sin(phi);
    dfdnu(5,3) = (thetadot*cos(phi))/cos(theta) - (psidot*sin(phi))/cos(theta);

    dfdnu(0,4) = x3dot*cos(phi)*cos(psi)*cos(theta) - x1dot*cos(psi)*sin(theta) + x2dot*cos(psi)*cos(theta)*sin(phi);
    dfdnu(1,4) = x3dot*cos(phi)*cos(theta)*sin(psi) - x1dot*sin(psi)*sin(theta) + x2dot*cos(theta)*sin(phi)*sin(psi);
    dfdnu(2,4) = - x1dot*cos(theta) - x3dot*cos(phi)*sin(theta) - x2dot*sin(phi)*sin(theta);
    dfdnu(3,4) = psidot*cos(phi)*(pow(tan(theta),2) + 1) + thetadot*sin(phi)*(pow(tan(theta),2) + 1);
    dfdnu(4,4) = 0;
    dfdnu(5,4) = (psidot*cos(phi)*sin(theta))/pow(cos(theta),2) + (thetadot*sin(phi)*sin(theta))/pow(cos(theta),2);

    dfdnu(0,5) = x3dot*(cos(psi)*sin(phi) - cos(phi)*sin(psi)*sin(theta)) - x2dot*(cos(phi)*cos(psi) + sin(phi)*sin(psi)*sin(theta)) - x1dot*cos(theta)*sin(psi);
    dfdnu(1,5) = x3dot*(sin(phi)*sin(psi) + cos(phi)*cos(psi)*sin(theta)) - x2dot*(cos(phi)*sin(psi) - cos(psi)*sin(phi)*sin(theta)) + x1dot*cos(psi)*cos(theta);

    dfdx.block(6,6,6,6) = dfdnu;

}

// Templated version of SlamLogLikelihood
// Note: templates normally should live in a template header (.hpp), but
//       since all instantiations of this template are used only in this
//       compilation unit, its definition can live here
template <typename Scalar>
static Scalar arucoLogLikelihood(const Eigen::Matrix<Scalar,Eigen::Dynamic,1> y, const Eigen::Matrix<Scalar,Eigen::Dynamic,1> & x, const Eigen::Matrix<Scalar,Eigen::Dynamic,1> & u, const SlamParameters & param)
{

    // Variables
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rJNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> eta(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> Thetanj(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> Rnj(3,3);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rJcNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> measurement_pixel(2,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> state_pixel(2,1);

    Eigen::Matrix<Scalar,Eigen::Dynamic,1> g;

    // Corner local measurements
    Scalar length = 0.166;
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> rJcJj(3,4);

    rJcJj.block(0,0,3,1) << -length/2, length/2, 0;
    rJcJj.block(0,1,3,1) << length/2, length/2, 0;
    rJcJj.block(0,2,3,1) << length/2, -length/2, 0;
    rJcJj.block(0,3,3,1) << -length/2, -length/2, 0;

    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> SR = param.measurement_noise*Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic>::Identity(2,2);
    Scalar cost = 0;
    eta = x.segment(6,6);
    //For each landmark seen
    int count = 0;
    for(int l = 0; l < param.landmarks_seen.size(); l++) {
        // *** State Predicted Landmark Location *** //
        rJNn = x.segment(12+param.landmarks_seen[l]*6,3);
        Thetanj = x.segment(12+3+param.landmarks_seen[l]*6,3);
        rpy2rot<Scalar>(Thetanj,Rnj);
        // For each corner of the landmark
        for(int c = 0; c < 4; c++) {
            // *** State Predicted Corner Pixel *** //
            rJcNn = Rnj*rJcJj.col(c) + rJNn;
            int w2p_flag = worldToPixel(rJcNn,eta,param.camera_param,state_pixel); // return [2x12]
            // *** Measurement Corner Pixel ***//
            measurement_pixel = y.segment(8*l + 2*c,2);
            // Sum up log gausian for each measurement
            if(w2p_flag == 0) {
                cost += logGaussian(measurement_pixel,state_pixel, SR);
                count++;
            } else {
                // std::cout << "outside view cone" << std::endl;
            }
        }
    }
    return cost;
}

#include <autodiff/forward/dual.hpp>
#include <autodiff/forward/dual/eigen.hpp>

double ArucoLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param)
{
    // Evaluate log N(y;h(x),R)
    return arucoLogLikelihood(y, x, u, param);
}


double ArucoLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g)
{
    Eigen::Matrix<autodiff::dual,Eigen::Dynamic,1> xdual = x.cast<autodiff::dual>();
    autodiff::dual fdual;
    auto t_start = std::chrono::high_resolution_clock::now();
    g = autodiff::gradient(arucoLogLikelihood<autodiff::dual>, wrt(xdual), at(y,xdual,u,param), fdual);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();
    // std::cout << "Time taken for gradient calc [s]: " << elapsed_time_ms/1000 << std::endl;
    return val(fdual);
}

double ArucoLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g, Eigen::MatrixXd &H)
{
    Eigen::Matrix<autodiff::dual2nd,Eigen::Dynamic,1> xdual = x.cast<autodiff::dual2nd>();
    autodiff::dual2nd fdual;
    auto t_start = std::chrono::high_resolution_clock::now();
    H = autodiff::hessian(arucoLogLikelihood<autodiff::dual2nd>, wrt(xdual), at(y,xdual,u,param), fdual, g);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();
    return val(fdual);
}

double arucoLogLikelihoodAnalytical::operator()(const Eigen::VectorXd y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g)
{

    // Variables
    Eigen::Matrix<double,Eigen::Dynamic,1> rJNn(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> eta(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> Thetanj(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> Rnj(3,3);
    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> Rnc(3,3);

    Eigen::Matrix<double,Eigen::Dynamic,1> Thetanc(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> rJcNn(3,1);

    Thetanc = x.segment(9,3);
    rpy2rot<double>(Thetanc,Rnc);
    Eigen::Matrix<double,Eigen::Dynamic,1> rCNn;
    rCNn = x.segment(6,3);

    Eigen::Matrix<double,Eigen::Dynamic,1> measurement_pixel(2,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> state_pixel(2,1);


    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> Sw2p;

    Eigen::MatrixXd dh_drjcc;
    Eigen::MatrixXd JrCNn;
    Eigen::MatrixXd JRnc;
    Eigen::MatrixXd JrPNn;
    Eigen::MatrixXd JRnm;


    Eigen::MatrixXd Jacobian = Eigen::MatrixXd::Zero(x.rows(),1);
    Eigen::MatrixXd dhdx = Eigen::MatrixXd::Zero(2,x.rows());

    Eigen::Matrix<double,Eigen::Dynamic,1> h = Eigen::Matrix<double,Eigen::Dynamic,1>::Zero(y.rows(),1);

    WorldToPixelAdaptor w2p;

    // Corner local measurements
    double length = 0.166;
    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> rJcJj(3,4);
    rJcJj << -length/2,length/2,length/2,-length/2,length/2,length/2,-length/2,-length/2,0,0,0,0;

    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> SR = param.measurement_noise*Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic>::Identity(2,2);
    double cost = 0;
    eta = x.segment(6,6);
    //For each landmark seenauto t_start = std::chrono::high_resolution_clock::now();
    int count = 0;
    Eigen::Matrix<double,Eigen::Dynamic,1> g_log(2,1);
    for(int l = 0; l < param.landmarks_seen.size(); l++) {
        // *** State Predicted Centre of Marker Location *** //
        rJNn = x.segment(12+param.landmarks_seen[l]*6,3);
        Thetanj = x.segment(12+3+param.landmarks_seen[l]*6,3);
        rpy2rot<double>(Thetanj,Rnj);
        // For each corner of the landmark
        for(int c = 0; c < 4; c++) {
            dhdx.fill(0);
            // *** State Predicted Location of Marker Corner Pixel *** //
            rJcNn = Rnj*rJcJj.col(c) + rJNn;
            int w2p_flag = w2p(rJcNn,eta,param.camera_param,state_pixel,Sw2p,dh_drjcc);

            // Jacobian of the camera position
            JrCNn = -dh_drjcc;
            // Jacobian of the corner of landmark
            JrPNn = dh_drjcc;

            // Camera position jacobian
            dhdx.block(0,6,2,3) = JrCNn;
            dhdx.block(0,12+6*param.landmarks_seen[l],2,3) = JrPNn;

            // Camera Rotation jacobian
            JRnc.resize(2,3);
            Eigen::MatrixXd S1(3,3);
            Eigen::MatrixXd S2(3,3);
            Eigen::MatrixXd S3(3,3);
            S1 << 0,0,0,0,0,-1,0,1,0;
            S2 << 0,0,1,0,0,0,-1,0,0;
            S3 << 0,-1,0,1,0,0,0,0,0;

            Eigen::MatrixXd RX = Eigen::MatrixXd::Zero(3,3);
            Eigen::MatrixXd RY= Eigen::MatrixXd::Zero(3,3);
            Eigen::MatrixXd RZ= Eigen::MatrixXd::Zero(3,3);

            rotx(eta(3),RX);
            roty(eta(4),RY);
            rotz(eta(5),RZ);
            Eigen::MatrixXd dPsi   =   RZ*S3*RY*RX;
            Eigen::MatrixXd dTheta =   RZ*RY*S2*RX;
            Eigen::MatrixXd dPhi   =   RZ*RY*RX*S1;

            JRnc.block(0,0,2,1) = dh_drjcc*Rnc*dPhi.transpose()*(rJcNn - rCNn);
            JRnc.block(0,1,2,1) = dh_drjcc*Rnc*dTheta.transpose()*(rJcNn - rCNn);
            JRnc.block(0,2,2,1) = dh_drjcc*Rnc*dPsi.transpose()*(rJcNn - rCNn);

            dhdx.block(0,9,2,3) = JRnc;

            // Landmark rotation jacobian
            rotx(Thetanj(0),RX);
            roty(Thetanj(1),RY);
            rotz(Thetanj(2),RZ);

            dPsi   =   RZ*S3*RY*RX;
            dTheta =   RZ*RY*S2*RX;
            dPhi   =   RZ*RY*RX*S1;
            JRnm.resize(2,3);
            JRnm.setZero();

            JRnm.block(0,0,2,1) = dh_drjcc*(dPhi*rJcJj.col(c));
            JRnm.block(0,1,2,1) = dh_drjcc*(dTheta*rJcJj.col(c));
            JRnm.block(0,2,2,1) = dh_drjcc*(dPsi*rJcJj.col(c));

            // Landmark jacobian
            dhdx.block(0,12+3+6*param.landmarks_seen[l],2,3) = JRnm;

            // *** Measurement Corner Pixel ***//
            measurement_pixel = y.segment(8*l + 2*c,2);
            // Sum up log gausian for each measurement
            if(w2p_flag == 0) {
                cost += logGaussian(measurement_pixel,state_pixel, SR, g_log);
                // std::cout << "g logGaussian" << g_log << std::endl;
                count++;

                Jacobian += dhdx.transpose()*-g_log;
            } else {
                // std::cout << "outside view cone" << std::endl;
            }

        }
    }

    g = Jacobian;
    return cost;
}
double arucoLogLikelihoodAnalytical::operator()(const Eigen::VectorXd y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g, Eigen::MatrixXd &H)
{
    double cost = operator()(y, x, u, param, g);
    H.resize(x.rows(),x.rows());
    H.setZero();
    return cost;
}

template <typename Scalar>
static Scalar pointLogLikelihood(const Eigen::Matrix<Scalar,Eigen::Dynamic,1> y, const Eigen::Matrix<Scalar,Eigen::Dynamic,1> & x, const Eigen::Matrix<Scalar,Eigen::Dynamic,1> & u, const SlamParameters & param)
{

    // Variables
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rJNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> eta(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> Thetanj(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> Rnj(3,3);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rJcNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> measurement_pixel(2,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> state_pixel(2,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> SR = param.measurement_noise*Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic>::Identity(2,2);
    Scalar cost = 0;
    eta = x.segment(6,6);

    //For each landmark seen
    int count = 0;
    // if(x.hasNaN()){
    //     std::cout << " X has nans: " << x << std::endl;
    //     assert(0);
    // }

    for(int j = 0; j < param.landmarks.size(); j++) {
        if(param.landmarks[j].isVisible) {
            // *** State Predicted Landmark Location *** //
            rJNn = x.segment(12+j*3,3);
            int w2p_flag = worldToPixel(rJNn,eta,param.camera_param,state_pixel); // return [2x12]
            // *** Measurement Point Pixel ***//
            measurement_pixel = param.landmarks[j].pixel_measurement;
            if(w2p_flag == 0) {
                double thresh = 25;
                cost += logGaussian(measurement_pixel,state_pixel, SR);
            }
        }
    }
    return cost;
}

double PointLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param)
{
    // Evaluate log N(y;h(x),R)
    return pointLogLikelihood(y, x, u, param);
}


double PointLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g)
{
    Eigen::Matrix<autodiff::dual,Eigen::Dynamic,1> xdual = x.cast<autodiff::dual>();
    autodiff::dual fdual;
    auto t_start = std::chrono::high_resolution_clock::now();
    g = autodiff::gradient(pointLogLikelihood<autodiff::dual>, wrt(xdual), at(y,xdual,u,param), fdual);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();
    // std::cout << "Time taken for gradient calc [s]: " << elapsed_time_ms/1000 << std::endl;
    return val(fdual);
}

double PointLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g, Eigen::MatrixXd & H)
{
    Eigen::Matrix<autodiff::dual2nd,Eigen::Dynamic,1> xdual = x.cast<autodiff::dual2nd>();
    autodiff::dual2nd fdual;
    auto t_start = std::chrono::high_resolution_clock::now();
    H = autodiff::hessian(pointLogLikelihood<autodiff::dual2nd>, wrt(xdual), at(y,xdual,u,param), fdual, g);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();
    // std::cout << "time taken for Hessian/Jacobian calc [s] : " << elapsed_time_ms/1000  << std::endl;
    return val(fdual);
}

double pointLogLikelihoodAnalytical::operator()(const Eigen::VectorXd y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g)
{

    // Variables
    Eigen::Matrix<double,Eigen::Dynamic,1> rJNn(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> eta(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> Thetanj(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> Rnj(3,3);
    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> Rnc(3,3);

    Eigen::Matrix<double,Eigen::Dynamic,1> Thetanc(3,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> rJcNn(3,1);

    Thetanc = x.segment(9,3);
    rpy2rot<double>(Thetanc,Rnc);
    Eigen::Matrix<double,Eigen::Dynamic,1> rCNn;
    rCNn = x.segment(6,3);

    Eigen::Matrix<double,Eigen::Dynamic,1> measurement_pixel(2,1);
    Eigen::Matrix<double,Eigen::Dynamic,1> state_pixel(2,1);


    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> Sw2p;

    Eigen::MatrixXd dh_drjcc;
    Eigen::MatrixXd JrCNn;
    Eigen::MatrixXd JRnc;
    Eigen::MatrixXd JrPNn;
    Eigen::MatrixXd JRnm;


    Eigen::MatrixXd Jacobian = Eigen::MatrixXd::Zero(x.rows(),1);
    Eigen::MatrixXd dhdx = Eigen::MatrixXd::Zero(2,x.rows());

    Eigen::Matrix<double,Eigen::Dynamic,1> h = Eigen::Matrix<double,Eigen::Dynamic,1>::Zero(y.rows(),1);

    WorldToPixelAdaptor w2p;

    Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> SR = param.measurement_noise*Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic>::Identity(2,2);
    double cost = 0;
    eta = x.segment(6,6);
    //For each landmark seenauto t_start = std::chrono::high_resolution_clock::now();
    int count = 0;
    Eigen::Matrix<double,Eigen::Dynamic,1> g_log(2,1);
    for(int j = 0; j < param.landmarks.size(); j++) {
        if(param.landmarks[j].isVisible) {
        // *** State Predicted Centre of Marker Location *** //

        // *** State Predicted Landmark Location *** //
        int w2p_flag = w2p(rJcNn,eta,param.camera_param,state_pixel,Sw2p,dh_drjcc); // return [2x12]
        rJNn = x.segment(12+j*3,3);

        dhdx.fill(0);
        // Jacobian of the camera position
        JrCNn = -dh_drjcc;
        // Jacobian of the corner of landmark
        JrPNn = dh_drjcc;

        // Camera position jacobian
        dhdx.block(0,6,2,3) = JrCNn;
        dhdx.block(0,12+3*j,2,3) = JrPNn;

        // Camera Rotation jacobian
        JRnc.resize(2,3);
        Eigen::MatrixXd S1(3,3);
        Eigen::MatrixXd S2(3,3);
        Eigen::MatrixXd S3(3,3);
        S1 << 0,0,0,0,0,-1,0,1,0;
        S2 << 0,0,1,0,0,0,-1,0,0;
        S3 << 0,-1,0,1,0,0,0,0,0;

        Eigen::MatrixXd RX = Eigen::MatrixXd::Zero(3,3);
        Eigen::MatrixXd RY= Eigen::MatrixXd::Zero(3,3);
        Eigen::MatrixXd RZ= Eigen::MatrixXd::Zero(3,3);

        rotx(eta(3),RX);
        roty(eta(4),RY);
        rotz(eta(5),RZ);
        Eigen::MatrixXd dPsi   =   RZ*S3*RY*RX;
        Eigen::MatrixXd dTheta =   RZ*RY*S2*RX;
        Eigen::MatrixXd dPhi   =   RZ*RY*RX*S1;

        JRnc.block(0,0,2,1) = dh_drjcc*Rnc*dPhi.transpose()*(rJcNn - rCNn);
        JRnc.block(0,1,2,1) = dh_drjcc*Rnc*dTheta.transpose()*(rJcNn - rCNn);
        JRnc.block(0,2,2,1) = dh_drjcc*Rnc*dPsi.transpose()*(rJcNn - rCNn);

        dhdx.block(0,9,2,3) = JRnc;

            // *** Measurement Point Pixel ***//
            measurement_pixel = param.landmarks[j].pixel_measurement;
            if(w2p_flag == 0) {
                double thresh = 25;
                cost += logGaussian(measurement_pixel,state_pixel, SR,g_log);
                Jacobian += dhdx.transpose()*-g_log;
            }
        }
    }

    g = Jacobian;
    return cost;
}

double pointLogLikelihoodAnalytical::operator()(const Eigen::VectorXd y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g, Eigen::MatrixXd &H)
{
    double cost = operator()(y, x, u, param, g);
    H.resize(x.rows(),x.rows());
    H.setZero();
    return cost;
}

template <typename Scalar>
static Scalar duckLogLikelihood(const Eigen::Matrix<Scalar,Eigen::Dynamic,1> y, const Eigen::Matrix<Scalar,Eigen::Dynamic,1> & x, const Eigen::Matrix<Scalar,Eigen::Dynamic,1> & u, const SlamParameters & param)
{

    // Variables
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rJNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rCNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> eta(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> Thetanj(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> Rnj(3,3);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> rJcNn(3,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> measurement_pixel(2,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> state_pixel(2,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> measurement_area(1,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,1> state_area(1,1);
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> SR = param.measurement_noise*Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic>::Identity(2,2);
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> SA = 2*Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic>::Identity(1,1);
    Scalar cost = 0;
    eta = x.segment(6,6);
    rCNn = x.segment(6,3);


    int count = 0;
    Scalar fx = param.camera_param.Kc.at<double>(0,0);
    Scalar fy = param.camera_param.Kc.at<double>(1,1);

    for(int j = 0; j < param.ducks.size(); j++) {
        if(param.ducks[j].isVisible) {
            // *** State Predicted Landmark Location *** //
            rJNn = x.segment(12+j*3,3);
            int w2p_flag = worldToPixel(rJNn,eta,param.camera_param,state_pixel);
            // *** Measurement Point Pixel ***//
            measurement_pixel = param.ducks[j].pixel_measurement;
            measurement_area(0) = 0.25*M_PI*param.ducks[j].keypoint.size*param.ducks[j].keypoint.size; // Area of a circle ? maf
            state_area(0) = (fx*fy*M_PI*0.0325*0.0325)/((rCNn-rJNn).squaredNorm());
            // measurement_area(0) = 3.142*param.ducks[j].keypoint.size*param.ducks[j].keypoint.size; // Area of a circle ? maf
            if(w2p_flag == 0) {
                cost += logGaussian(measurement_pixel,state_pixel, SR); // + logGaussian(measurement_area,state_area,SA);
            }
        }
    }
    return cost;
}

double DuckLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param)
{
    // Evaluate log N(y;h(x),R)
    return duckLogLikelihood(y, x, u, param);
}


double DuckLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g)
{
    Eigen::Matrix<autodiff::dual,Eigen::Dynamic,1> xdual = x.cast<autodiff::dual>();
    autodiff::dual fdual;
    auto t_start = std::chrono::high_resolution_clock::now();
    g = autodiff::gradient(duckLogLikelihood<autodiff::dual>, wrt(xdual), at(y,xdual,u,param), fdual);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();
    // std::cout << "Time taken for gradient calc [s]: " << elapsed_time_ms/1000 << std::endl;
    return val(fdual);
}

double DuckLogLikelihood::operator()(const Eigen::VectorXd & y, const Eigen::VectorXd & x, const Eigen::VectorXd & u, const SlamParameters & param, Eigen::VectorXd &g, Eigen::MatrixXd & H)
{
    Eigen::Matrix<autodiff::dual2nd,Eigen::Dynamic,1> xdual = x.cast<autodiff::dual2nd>();
    autodiff::dual2nd fdual;
    auto t_start = std::chrono::high_resolution_clock::now();
    H = autodiff::hessian(duckLogLikelihood<autodiff::dual2nd>, wrt(xdual), at(y,xdual,u,param), fdual, g);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();
    // std::cout << "time taken for Hessian/Jacobian calc [s] : " << elapsed_time_ms/1000  << std::endl;
    return val(fdual);
}






