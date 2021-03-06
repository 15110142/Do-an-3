#include <limits>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QAudio>
#include <QSettings>
#include <QAudioDeviceInfo>
//#include <QGLViewer/qglviewer.h>
//#if defined(__APPLE__) || defined(MACOSX)
//#   include <glut.h>
//#elif defined(__linux)
//#   include <GL/glut.h>
//#else
//#   error ERROR Unsupported OS
//#endif
#include "io/TglMeshReader.hpp"
#include "utils/macros.h"
#include "utils/term_msg.h"
#include "AudioProducer.h"

#include <iostream>
#include <QtEndian>
//#include <time.h>
#include <thread>
// #include <mutex>

// std::mutex write_buffer_lock;

static const int SR = 44100;
double duration = 3;

using namespace std;

class ClickSoundViewer //: public QGLViewer
{
    public:
        ClickSoundViewer(const char* inifile);

////////////////////////////////////////////////////////
        void sim_click(const QPoint& selected_point);
        void generate_sounds();
        void generate_continuous();
        bool                gui;
        bool                continuous;
        std::vector<double> whole_soundBuffer;
///////////////////////////////////////////////////////


        ~ClickSoundViewer()
        {
            delete audio_;
        }

    protected:
        //void init();
        //void draw();
        //void keyPressEvent(QKeyEvent* e);
        //void drawWithNames();
        //void postSelection(const QPoint&);

    private:
        //void init_gl();
       void load_mesh(const QString& meshfile);

       // void draw_mesh();
        //void draw_obj();
        //void draw_triangle_normal();
        void run_thread(double collision_time, int selTriId, Vector3d nml,Point3d CamPos,float amp,int index_t);
    private:
        QDir                dataDir_;
        bool                wireframe_;
        int                 selTriId_;

///////////////////////////////////////////////////////////////////////////////
        Point3d             default_CamPos;
        QString             selected_Tris;
        QString             amplitudes;
        QString             collision_times;
        QString             norm1s;
        QString             norm2s;
        QString             norm3s;
        QString             camXs;
        QString             camYs;
        QString             camZs;
///////////////////////////////////////////////////////////////////////////////


        AudioProducer*      audio_;

        TriangleMesh<double>    mesh_;
};

// ------------------------------------------------------------

ClickSoundViewer::ClickSoundViewer(const char* inifile) //:
        //wireframe_(false), selTriId_(-1), audio_(NULL)
{
    cout<<"!!!!"<<endl;
	QString iniPath(inifile);
    QFileInfo checkConfig(iniPath);

    if ( !checkConfig.exists() )
    {
        PRINT_ERROR("The INI file %s doesn't exist\n", inifile);
        SHOULD_NEVER_HAPPEN(-1);
    }

	cout<<"1"<<endl;
    // load the settings
    QSettings settings(iniPath, QSettings::IniFormat);
    //cout << "HELLO: " << settings.value("mesh/surface_mesh").toString().toStdString() << endl;
    //cout << "       \"" << settings.value("audio/device2").toString().toStdString() << '"' << endl;

    dataDir_ = checkConfig.absoluteDir(); //.absolutePath().toStdString() << endl;

	cout<<"2"<<endl;
    ////// load the files
    // - triangle mesh
    load_mesh(settings.value("mesh/surface_mesh").toString());

    audio_ = new AudioProducer(settings, dataDir_);

///////////////////////////////////////////////////////////////////////////////
    default_CamPos.x = settings.value("camera/x").toDouble();
    default_CamPos.y = settings.value("camera/y").toDouble();
    default_CamPos.z = settings.value("camera/z").toDouble();

    selected_Tris = settings.value("collisions/ID").toString();
    amplitudes = settings.value("collisions/amplitude").toString();
    collision_times = settings.value("collisions/time").toString();

    gui = settings.value("gui/gui").toBool();
    continuous = settings.value("audio/continuous").toBool();
    norm1s = settings.value("collisions/norm1").toString();
    norm2s = settings.value("collisions/norm2").toString();
    norm3s = settings.value("collisions/norm3").toString();
    camXs = settings.value("collisions/camX").toString();
    camYs = settings.value("collisions/camY").toString();
    camZs = settings.value("collisions/camZ").toString();
///////////////////////////////////////////////////////////////////////////////
}


void ClickSoundViewer::load_mesh(const QString& meshfile)
{
    QString filename;
    {
        QFileInfo fInfo(meshfile);
        filename = fInfo.isRelative() ? dataDir_.filePath(meshfile) : meshfile;
    }

    PRINT_MSG("Load the mesh file: %s\n", filename.toStdString().c_str());
    if ( _FAILED( MeshObjReader::read(filename.toStdString().c_str(), mesh_) ) )
    {
        PRINT_ERROR("Cannot load the mesh file: %s\n",
                    filename.toStdString().c_str());
        SHOULD_NEVER_HAPPEN(-1);
    }
    if ( !mesh_.has_normals() ) mesh_.generate_pseudo_normals();
}
/*
void ClickSoundViewer::init()
{

		cout<<"init"<<endl;
		//    init_gl();      // initialize OpenGL

  //  resize(1024, 756);
    // const qglviewer::Vec camPos(0,1,10);
    // camera()->setPosition(camPos);
    //camera()->setZNearCoefficient(0.0001f);
    //camera()->setZClippingCoefficient(100.f);
}

void ClickSoundViewer::draw()
{
    draw_mesh();
    if ( selTriId_ >= 0 ) draw_triangle_normal();
}

void ClickSoundViewer::draw_obj()
{
    const vector<Point3d>&  vtx = mesh_.vertices();
    const vector<Tuple3ui>& tgl = mesh_.surface_indices();
    const vector<Vector3d>& nml = mesh_.normals();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glVertexPointer(3, GL_DOUBLE, 0, (const GLvoid*)(vtx.data()));
    glNormalPointer(GL_DOUBLE, 0, (const GLvoid*)(nml.data()));
    glDrawElements(GL_TRIANGLES, tgl.size()*3, GL_UNSIGNED_INT, (const GLvoid*)(tgl.data()));
}

void ClickSoundViewer::draw_mesh()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor3f(1.f, 1.f, 0.f);
    glEnable(GL_LIGHTING);

    draw_obj();
    if ( wireframe_ )
    {
        glLineWidth(2.);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.8f, 0.8f, 0.8f);
        glDisable(GL_LIGHTING);
        draw_obj();
    }
}

void ClickSoundViewer::draw_triangle_normal()
{
    const int tid = selTriId_;
    const vector<Point3d>&  vtx = mesh_.vertices();
    const vector<Tuple3ui>& tgl = mesh_.surface_indices();

    const Point3d ctr  = (vtx[tgl[tid].x] + vtx[tgl[tid].y] + vtx[tgl[tid].z])*(1./3.);
    Vector3d nml = Triangle<double>::weighted_normal( vtx[tgl[tid].x], vtx[tgl[tid].y], vtx[tgl[tid].z] );
    double area = nml.normalize2();
    const double len = 0.05;     ///// HARD coded value here!!!
    const Point3d end  = ctr + nml * len;

    glColor3f(1.f, 0.f, 0.f);
    drawArrow( qglviewer::Vec(end.x, end.y, end.z),
               qglviewer::Vec(ctr.x, ctr.y, ctr.z),
               sqrt(area)*0.8 );
}

void ClickSoundViewer::keyPressEvent(QKeyEvent* e)
{
    const Qt::KeyboardModifiers modifiers = e->modifiers();

    if ( e->key() == Qt::Key_W && modifiers == Qt::NoButton )
    {
        wireframe_ = !wireframe_;
        updateGL();
    }
    else
        QGLViewer::keyPressEvent(e);
}

void ClickSoundViewer::drawWithNames()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    glVertexPointer(3, GL_DOUBLE, 0, (const GLvoid*)(mesh_.vertices().data()));
    const vector< Tuple3ui >& tgl = mesh_.triangles();
    for(size_t i = 0;i < tgl.size();++ i)
    {
        glPushName(i);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (const GLvoid*)&tgl[i]);
        glPopName();
    }
}

void ClickSoundViewer::postSelection(const QPoint& point)
{
    // uncomment to print the selected QPoint
    // int x=point.x();
    // int y=point.y();
    // cout<<x<<endl;
    // cout<<y<<endl;
    selTriId_ = selectedName(); // Thin method returns a int type

    //cout << "selected triangle ID:  " << selTriId_ << endl;

    //// Now synthesize sound and play
    {
        qglviewer::Vec cam = camera()->position();
        const Point3d camPos( cam.x, cam.y, cam.z );

        const vector<Point3d>&  vtx = mesh_.vertices();
        const vector<Tuple3ui>& tgl = mesh_.surface_indices();
        Vector3d nml = Triangle<double>::normal(
                vtx[tgl[selTriId_].x],
                vtx[tgl[selTriId_].y],
                vtx[tgl[selTriId_].z] );
        nml.normalize();
        // nml*=10;
        audio_->play( mesh_.triangle_ids(selTriId_), nml, camPos, 1 );
    }

    if ( selTriId_ >= 0 ) updateGL();
}

void ClickSoundViewer::init_gl()
{
#ifdef __linux
    int dummy = 0;
    glutInit(&dummy, NULL);
#endif
    const GLfloat GLOBAL_AMBIENT[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    const GLfloat SPECULAR_COLOR[] = { 0.6f, 0.6f, 0.6f, 1.0 };

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, GLOBAL_AMBIENT);
    const GLfloat ambientLight[] = { 0.f, 0.f, 0.f, 1.0f };
    const GLfloat diffuseLight[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    const GLfloat specularLight[] = { 1.f, 1.f, 1.f, 1.0f };
    const GLfloat position[] = { -0.5f, 1.0f, 0.4f, 1.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    glLightfv(GL_LIGHT0, GL_POSITION, position);

    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50.);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, SPECULAR_COLOR);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // antialiasing
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


//////////////////////////////////////////////////////////////////////////////
// use sim_click to simulate a mouse click
void ClickSoundViewer::sim_click(const QPoint& selected_point)
{
    select(selected_point);
    //select will call postSelection() at last
    //segmentationfault if the selected_point is not on the object
}
*/
void ClickSoundViewer::generate_sounds()
{
  // generate wav file for every selected Tri specified in .ini file
    QStringList selTriIds = selected_Tris.split(" ");
    QStringList Amps = amplitudes.split(" ");

    if (selTriIds.size()!=Amps.size())
        PRINT_ERROR("Number of selected Tri should equal to number of amplitudes");

    for (int i = 0; i < selTriIds.size(); ++i){
        int selTriId = selTriIds.at(i).toInt();
        double amp = Amps.at(i).toDouble();

        const vector<Point3d>&  vtx = mesh_.vertices();
        const vector<Tuple3ui>& tgl = mesh_.surface_indices();
        Vector3d nml = Triangle<double>::normal(
                vtx[tgl[selTriId].x],
                vtx[tgl[selTriId].y],
                vtx[tgl[selTriId].z] );
        nml.normalize();
        audio_->play( mesh_.triangle_ids(selTriId), nml, default_CamPos, amp );
    }
}

void ClickSoundViewer::run_thread(double collision_time,int selTriId, Vector3d nml,Point3d CamPos,float amp,int index_t)
{

    audio_->single_channel_synthesis(mesh_.triangle_ids(selTriId), nml, CamPos, amp, index_t);

    // std::lock_guard<std::mutex> lock(write_buffer_lock);
    for (int j=0; j < audio_->soundBuffer_[index_t].size(); ++j){
        whole_soundBuffer.at(SR*collision_time*audio_->format_.channelCount() + j) += audio_->soundBuffer_[index_t].at(j);
    }
}

void ClickSoundViewer::generate_continuous()
{   
    

    QStringList selTriIds = selected_Tris.split(" ");
    QStringList Amps = amplitudes.split(" ");
    QStringList times = collision_times.split(" ");
    QStringList normal1s = norm1s.split(" ");
    QStringList normal2s = norm2s.split(" ");
    QStringList normal3s = norm3s.split(" ");
    QStringList camPosXs = camXs.split(" ");
    QStringList camPosYs = camYs.split(" ");
    QStringList camPosZs = camZs.split(" ");

    double final_time = times.at(times.size()-1).toDouble();//time of the last collision

    //const qint64 len = ( (SR*final_time* audio_->format_.channelCount()* format_.sampleSize() / 8 + SR * audio_->TS * audio_->format_.channelCount()) * audio_->format_.sampleSize() / 8);
    const qint64 len = SR* (int)duration * audio_->format_.sampleSize() / 8;
    QByteArray buffer;
    buffer.resize(len);

//    std::vector<double> whole_soundBuffer;
    //TODO: convert resize(number) number from double to int?
    cout<<"ChannelCount: "<<audio_->format_.channelCount()<<endl;
    //whole_soundBuffer.resize( SR*final_time* audio_->format_.channelCount() + SR * audio_->TS * audio_->format_.channelCount() );
    whole_soundBuffer.resize( std::max(SR*final_time* audio_->format_.channelCount() + SR * audio_->TS * audio_->format_.channelCount(),SR*duration) );

    //cout<<"1\n";
    cout<<"Num clicks: "<<selTriIds.size()<<"\n";
    
    std::thread ts[NUM_THREADS];
    int running_threads = 0;

    for (int i = 0; i < selTriIds.size(); ++i){
        int index_t = i%NUM_THREADS;
        int selTriId = selTriIds.at(i).toInt();
        double amp = Amps.at(i).toDouble()/16;
        double collision_time = times.at(i).toDouble();

        if(collision_time > duration)
            break;

        const vector<Point3d>&  vtx = mesh_.vertices();
        const vector<Tuple3ui>& tgl = mesh_.surface_indices();
        Vector3d nml = Triangle<double>::normal(
                vtx[tgl[selTriId].x],
                vtx[tgl[selTriId].y],
                vtx[tgl[selTriId].z] );
        // Vector3d nml(normal1s.at(i).toDouble(),normal2s.at(i).toDouble(),normal3s.at(i).toDouble());
        nml.normalize();

        Point3d CamPos;
        CamPos.x = camPosXs.at(i).toDouble();
        CamPos.y = camPosYs.at(i).toDouble();
        CamPos.z = camPosZs.at(i).toDouble();
        //cout<<"------\n";
        ts[index_t] = std::thread(&ClickSoundViewer::run_thread,this,collision_time, selTriId, nml, CamPos, amp, index_t);
        running_threads+=1;

        // if( i%NUM_THREADS == NUM_THREADS-1 || i==selected_Tris.size()-1){
        if( (i==selTriIds.size()-1) || i%NUM_THREADS==NUM_THREADS-1  ){
            for(int nt =0; nt< running_threads; ++nt){
                ts[nt].join();
            } 
            running_threads = 0;
        }
//        audio_->single_channel_synthesis(mesh_.triangle_ids(selTriId), nml, CamPos, amp, index_t);
        
        //cout<<"======\n";

//        for (int j=0; j < audio_->soundBuffer_[index_t].size(); ++j){
//            whole_soundBuffer.at(SR*collision_time*audio_->format_.channelCount() + j) += audio_->soundBuffer_[index_t].at(j);
    }




// new code with ch=1 starts from here
    std::vector<double> whole_soundBuffer_crop(&whole_soundBuffer[0],&whole_soundBuffer[SR*duration]);

    unsigned char *ptr = reinterpret_cast<unsigned char *>(buffer.data());
    const int channelBytes = audio_->format_.sampleSize() / 8;
    for(int ti = 0;ti < whole_soundBuffer_crop.size();++ ti)
    {
        const qint16 value = static_cast<qint16>( whole_soundBuffer_crop[ti] * 32767 );
        qToLittleEndian<qint16>(value, ptr);
        ptr += channelBytes;
    }

    audio_->generate_continuous_wav(buffer,whole_soundBuffer_crop);
// end

    // unsigned char *ptr = reinterpret_cast<unsigned char *>(buffer.data());
    // const int channelBytes = audio_->format_.sampleSize() / 8;
    // for(int ti = 0;ti < whole_soundBuffer.size();++ ti)
    // {
    //     const qint16 value = static_cast<qint16>( whole_soundBuffer[ti] * 32767 );
    //     qToLittleEndian<qint16>(value, ptr);
    //     ptr += channelBytes;
    // }
    //
    // audio_->generate_continuous_wav(buffer,whole_soundBuffer);


}

///////////////////////////////////////////////////////////////////////////////


static void list_audio_devices()
{
    cout << "Detected audio devices: " << endl;
    foreach(const QAudioDeviceInfo &deviceInfo,
            QAudioDeviceInfo::availableDevices(QAudio::AudioOutput))
        cout << "  + \"" << deviceInfo.deviceName().toStdString() << '"' << endl;
}

int main(int argc, char* argv[])
{
    //clock_t start = clock();
    if ( argc < 2 )
    {
        cerr << "Invalid arguments!" << endl;
        cerr << "Usage: " << argv[0] << " [list | .ini file]" << endl;
        return 1;
    }

    // list audio output devices
    if ( !strcmp(argv[1], "list") )
    {
        list_audio_devices();
        return 0;
    }

    cout<<"generating QApp and ClickSoundViewer\n";
    
	QCoreApplication app(argc, argv);
	cout<<argv[3]<<endl;
    ClickSoundViewer viewer(argv[3]);
	cout<<"finished!\n"<<endl;
    //viewer.setWindowTitle(title.c_str());

////////////////////////////////////////////////////////////////////////
    //if(viewer.gui){
    //    viewer.show();
    //    return app.exec();
    //}
    // QPoint selected_point;
    // selected_point.setX(515);
    // selected_point.setY(384);
    // viewer.sim_click(selected_point);

    // const Point3d camPos(100,100,100);
    // const int selTriId_=500;
    // viewer.sim_click2(camPos, selTriId_);
    //cout <<"start to generate sound\n";
    if(!viewer.continuous){
        viewer.generate_sounds();
    }
    else
        viewer.generate_continuous();
////////////////////////////////////////////////////////////////////////

    //app.exit(0);
    //clock_t end = clock();
    //float sec = (float)((float)end-(float)start)/(float)CLOCKS_PER_SEC;
    //cout<<"time usage: "<<sec<<"\n";

    return 0;

}
