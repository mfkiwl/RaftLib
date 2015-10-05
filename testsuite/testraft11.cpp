#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cstdint>
#include <iostream>
#include <raft>

using cvm= typename cv::Mat;


template < class T > class source : public raft::kernel
{
public:
   source()
   {
      output.addPort< cvm >( "0" );
      if( not stream1.isOpened() ) 
      { 
         std::cout << "cannot open camera\n";
         exit( EXIT_FAILURE );
      }
   }

   virtual ~source() = default;

   virtual raft::kstatus run()
   {
      auto &frame( output[ "0" ].template allocate< cvm >() );
      stream1.read( frame );
      output[ "0" ].send();
      return( raft::proceed );
   }

private:
   cv::VideoCapture stream1 = { 0 };
};

template < class T > class canny : public raft::kernel
{
public:
   canny()
   {
      input.addPort< T >( "0" );
      output.addPort< T >( "0" );
   }

   virtual ~canny() = default;

   virtual raft::kstatus run()
   {
      auto &img_in( input[ "0" ].template peek< T >() );
      auto &out( output[ "0" ].template allocate< T >() );
      cv::cvtColor( img_in, 
                    out, 
                    CV_BGR2GRAY );
      cv::blur(     out, 
                    out, 
                    cv::Size( 3, 3) );
      cv::Canny( img_in, 
                 out, 
                 thresh, 
                 thresh*2, 
                 3 
           );
      img_in.release();
      input[ "0" ].recycle();
      output[ "0" ].send();
      return( raft::proceed );
   }

private:
   const std::int8_t thresh = 100;
};

template < class T > class findcontours : public raft::kernel
{
public:
   findcontours()
   {
      input.addPort<  T >( "0" );
      output.addPort< T >( "0" );
   }

   virtual ~findcontours() = default;

   virtual raft::kstatus run()
   {
      auto &img_in( input[ "0" ].template peek< T >() );
      auto &out( output[ "0" ].template allocate< T >() );
      cv::findContours( img_in,
                        contours,
                        hierarchy,
                        CV_RETR_TREE,
                        cv::CHAIN_APPROX_SIMPLE,
                        cv::Point() );
      out = cv::Mat::zeros( img_in.size(), 
                            CV_8UC3 );
      for( auto i( 0 ); std::isless( i, contours.size() ); i++ )
      {
         cv::Scalar color( 
            cv::Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) ) );

         cv::drawContours( out, 
                           contours, 
                           i, 
                           color, 
                           2, 
                           8, 
                           hierarchy, 
                           0, 
                           cv::Point() 
                         );
      }
      img_in.release();
      input[ "0" ].recycle();
      output[ "0" ].send();
      return( raft::proceed );
   }

private:
   std::vector< std::vector< cv::Point > > contours;
   std::vector< cv::Vec4i>                 hierarchy;
   cv::RNG rng = { 12345 };
};

static std::uint64_t frames = 0;
static double        start  = 0.0;
static double        end    = 0.0;

template < class T > class display : public raft::kernel
{
public:
   display()          
   {
      input.addPort< cvm >( "0" );
   }
   
   virtual ~display() = default;
   
   virtual raft::kstatus run()
   {
      auto &frame( input[ "0" ].template peek< cvm >() );
      cv::imshow( "cam", frame );
      /** decrement count within frame so it'll deallocate before recycle **/
      frame.release();
      input[ "0" ].recycle();
      frames++;
      if( frames % 200 == 0 )
      {
         end = system_clock->getTime();
         const auto fps( frames / (end - start) );
         std::cout << fps << "\n";
      }
      return( raft::proceed );
   }
  
};

extern Clock *system_clock;

int main() 
{
   cv::namedWindow( "cam", cv::WINDOW_NORMAL );
   auto a(
   raft::map.link( 
      raft::kernel::make< source< cvm > >(), 
      raft::kernel::make< canny< cvm > >() 
   ) );
   auto b( raft::map.link( 
      &a.getDst(),
      raft::kernel::make< findcontours< cvm > >() 
   ) );
   raft::map.link( 
      &b.getDst(),
      raft::kernel::make< display< cvm > >() 
   );
   /** global time check **/
   start =  system_clock->getTime();
   raft::map.exe();
   return( EXIT_SUCCESS );
}

