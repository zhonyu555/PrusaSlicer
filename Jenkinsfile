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
      steps {
        cmakeBuild(installation: 'cmake', buildType: 'debug', cleanBuild: true, buildDir: 'build')
        build 'cmake'
        sh '''cmake {
            cmakeInstallation(\'InSearchPath\')
            generator(\'Unix Makefiles\')
            cleanBuild()
            sourceDir(\'src\')
            buildDir(\'target\')
            args(\'foo\')
            args(\'bar\')
            buildToolStep {
                vars(\'KEY\', \'VALUE\')
                useCmake()
            }
            buildToolStep {
                useCmake(false)
            }
        }'''
        }
      }

    }
  }