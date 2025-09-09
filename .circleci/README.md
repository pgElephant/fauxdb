# Circle CI Configuration for FauxDB

This directory contains comprehensive Circle CI configuration files for automated build, test, deployment, and monitoring of the FauxDB project.

## Configuration Files

### 1. config.yml (Main Pipeline)
Primary CI/CD pipeline with the following jobs:

**Build & Test Jobs:**
- `build_and_test`: Compiles FauxDB with PostgreSQL backend, runs unit tests
- `security_scan`: Static security analysis with Bandit and safety checks
- `performance_test`: Benchmarking and performance regression testing
- `code_coverage`: Generates and uploads coverage reports to Codecov

**Quality Assurance:**
- `lint_and_format`: Runs clang-format and static analysis
- `documentation`: Builds and validates documentation
- `integration_test`: End-to-end testing with PostgreSQL integration

**Approval & Deployment:**
- Manual approval gates for production deployments
- Automated staging deployments on develop branch
- Production deployments on main branch

### 2. continue_config.yml (Dynamic Builds)
Conditional pipeline execution based on changed files:

- **Components Detection**: Automatically detects which parts of the codebase changed
- **Selective Testing**: Runs only relevant tests for changed components
- **Resource Optimization**: Reduces build time by skipping unchanged components
- **Smart Workflows**: Different pipelines for core changes vs documentation updates

### 3. docker_config.yml (Container Builds)
Multi-stage Docker image building and deployment:

**Build Stages:**
- `build_base_image`: Creates optimized base image with dependencies
- `build_app_image`: Builds application with multi-stage optimization
- `security_scan_image`: Scans Docker images for vulnerabilities
- `performance_test_image`: Tests container performance and resource usage

**Registry Operations:**
- Pushes to Docker Hub and AWS ECR
- Tags with commit SHA and semantic versioning
- Implements image caching for faster builds

### 4. deploy_config.yml (Kubernetes Deployment)
Production-ready Kubernetes deployments:

**Infrastructure:**
- Staging and production namespace management
- ConfigMaps for application configuration
- Secrets management for sensitive data
- Service and Ingress configuration

**Deployment Features:**
- Blue-green deployments with rollback capability
- Health checks and readiness probes
- Resource limits and auto-scaling
- Load balancing and service discovery

### 5. monitoring_config.yml (Observability Stack)
Complete monitoring and logging setup:

**Prometheus Stack:**
- Metrics collection from FauxDB instances
- PostgreSQL monitoring with custom exporters
- Kubernetes cluster monitoring
- Custom alerting rules for critical metrics

**Grafana Dashboards:**
- Real-time performance monitoring
- Request rate and latency tracking
- Error rate and availability metrics
- Resource utilization visualization

**ELK Stack:**
- Centralized log aggregation with Elasticsearch
- Kibana dashboards for log analysis
- Filebeat for container log collection
- Custom log parsing and indexing

## Setup Instructions

### 1. Initial Configuration

```bash
# Clone the repository
git clone <repository-url>
cd fauxdb

# Set up Circle CI context variables
# In Circle CI dashboard, create contexts:
# - aws-staging: AWS credentials for staging
# - aws-production: AWS credentials for production
# - docker-registry: Docker Hub/ECR credentials
```

### 2. Required Environment Variables

Create the following environment variables in Circle CI:

**AWS Deployment:**
```
AWS_ACCESS_KEY_ID=<your-aws-access-key>
AWS_SECRET_ACCESS_KEY=<your-aws-secret-key>
AWS_DEFAULT_REGION=us-west-2
```

**Docker Registry:**
```
DOCKER_USERNAME=<docker-hub-username>
DOCKER_PASSWORD=<docker-hub-password>
ECR_REGISTRY=<aws-ecr-registry-url>
```

**Database:**
```
POSTGRES_PASSWORD=<secure-password>
POSTGRES_USER=fauxdb
POSTGRES_DB=fauxdb
```

**Monitoring:**
```
CODECOV_TOKEN=<codecov-token>
SONAR_TOKEN=<sonarcloud-token>
```

### 3. Kubernetes Setup

Before running deployment pipelines, ensure your Kubernetes cluster is ready:

```bash
# Create EKS cluster (example)
eksctl create cluster --name fauxdb-cluster --region us-west-2 --nodes 3

# Install NGINX Ingress Controller
kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/controller-v1.8.1/deploy/static/provider/aws/deploy.yaml

# Install cert-manager for TLS
kubectl apply -f https://github.com/cert-manager/cert-manager/releases/download/v1.13.0/cert-manager.yaml
```

### 4. Enable Workflows

In your Circle CI project settings:

1. **Enable Dynamic Config**: Required for continue_config.yml
2. **Add SSH Keys**: For repository access
3. **Set up Contexts**: Link AWS, Docker, and monitoring contexts
4. **Configure Orbs**: Enable AWS CLI, Kubernetes, and other orbs

## Workflow Triggers

### Automatic Triggers

- **Push to `develop`**: Triggers staging deployment pipeline
- **Push to `main`**: Triggers production deployment pipeline  
- **Pull Requests**: Runs build, test, and security scans
- **Scheduled**: Nightly security scans and dependency updates

### Manual Triggers

- **Production Deployment**: Requires manual approval after staging
- **Rollback**: Can be triggered manually via Circle CI API
- **Performance Tests**: Manually triggered for capacity planning

## Monitoring and Alerts

### Built-in Monitoring

The configuration automatically sets up:

- **Application Metrics**: Request rates, latency, errors
- **Infrastructure Metrics**: CPU, memory, disk usage
- **Database Metrics**: Connection pool, query performance
- **Container Metrics**: Image vulnerabilities, resource usage

### Alert Channels

Configure alert destinations in your monitoring setup:

```yaml
# Example Slack integration
alertmanager:
  config:
    receivers:
      - name: 'slack-alerts'
        slack_configs:
          - api_url: 'https://hooks.slack.com/services/...'
            channel: '#alerts'
            title: 'FauxDB Alert'
```

## Troubleshooting

### Common Issues

1. **Build Failures**
   ```bash
   # Check build logs in Circle CI dashboard
   # Verify all dependencies are properly specified
   # Ensure CMake configuration is correct
   ```

2. **Deployment Failures**
   ```bash
   # Check Kubernetes cluster connectivity
   kubectl cluster-info
   
   # Verify secrets and configmaps
   kubectl get secrets -n fauxdb-staging
   kubectl get configmaps -n fauxdb-staging
   ```

3. **Monitoring Setup Issues**
   ```bash
   # Check Helm releases
   helm list -n monitoring
   
   # Verify Prometheus targets
   kubectl port-forward svc/prometheus-server 9090:80 -n monitoring
   # Visit http://localhost:9090/targets
   ```

### Debug Commands

```bash
# Check Circle CI configuration validity
circleci config validate .circleci/config.yml

# Test jobs locally
circleci local execute --job build_and_test

# Check deployment status
kubectl get pods -n fauxdb-staging
kubectl logs -f deployment/fauxdb -n fauxdb-staging
```

## Security Considerations

1. **Secrets Management**: All sensitive data stored in Circle CI contexts
2. **Image Scanning**: Automated vulnerability scanning in Docker pipeline
3. **Network Policies**: Kubernetes network segmentation
4. **RBAC**: Proper role-based access control for deployments
5. **TLS**: End-to-end encryption for all communications

## Performance Optimization

1. **Caching**: Docker layer and dependency caching enabled
2. **Parallel Execution**: Jobs run in parallel where possible
3. **Resource Limits**: Optimized executor sizes for each job type
4. **Selective Testing**: Only run tests for changed components

## Maintenance

### Regular Tasks

1. **Update Dependencies**: Scheduled dependency updates
2. **Review Metrics**: Weekly performance and security reviews
3. **Capacity Planning**: Monthly resource utilization analysis
4. **Backup Verification**: Test restore procedures quarterly

### Upgrade Path

1. **Circle CI Orbs**: Update to latest versions monthly
2. **Kubernetes**: Follow managed cluster upgrade schedule
3. **Monitoring Stack**: Upgrade Prometheus/Grafana quarterly
4. **Security Tools**: Keep security scanners updated

## Support

For issues with the CI/CD pipeline:

1. Check Circle CI dashboard for build logs
2. Review Kubernetes events: `kubectl get events --sort-by='.lastTimestamp'`
3. Monitor application logs in Kibana
4. Check Grafana dashboards for performance issues

## Contributing

When modifying CI/CD configuration:

1. Test changes in a feature branch
2. Validate YAML syntax: `yamllint .circleci/`
3. Review security implications
4. Update documentation accordingly
5. Test rollback procedures
