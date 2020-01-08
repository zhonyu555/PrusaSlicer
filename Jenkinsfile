pipeline {
  agent any
  stages {
    stage('mk_build') {
      steps {
        sh '''pwd
ls
mkdir -p build
cd build
pwd
ls'''
      }
    }

    stage('vypis_slozek') {
      steps {
        sh '''pwd
ls'''
      }
    }

    stage('cmake') {
      parallel {
        stage('cmake') {
          steps {
            cmake(installation: 'cmake', arguments: 'install -S $Workspace -B build ')
          }
        }

        stage('cmake 2') {
          steps {
            cmakeBuild(installation: 'cmake', buildDir: 'build', cleanBuild: true, buildType: 'debug')
          }
        }

      }
    }

  }
}